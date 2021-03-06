/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2014 Rockchip Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc.
 */

#include <arch/cache.h>
#include <stdint.h>

#include "dwc2.h"
#include "dwc2_private.h"

#include "base/algorithm.h"
#include "base/die.h"
#include "base/io.h"
#include "base/time.h"
#include "base/xalloc.h"

static void dummy(UsbDevHc *controller)
{
}

static void dwc2_reinit(UsbDevHc *controller)
{
	dwc2_reg_t *reg = DWC2_REG(controller);
	gusbcfg_t gusbcfg = { .d32 = 0 };
	grstctl_t grstctl = { .d32 = 0 };
	gintsts_t gintsts = { .d32 = 0 };
	gahbcfg_t gahbcfg = { .d32 = 0 };
	grxfsiz_t grxfsiz = { .d32 = 0 };
	ghwcfg3_t hwcfg3 = { .d32 = 0 };
	hcintmsk_t hcintmsk = { .d32 = 0 };
	gtxfsiz_t gnptxfsiz = { .d32 = 0 };
	gtxfsiz_t hptxfsiz = { .d32 = 0 };

	const int timeout = 10000;
	int i, fifo_blocks, tx_blocks;

	/* Wait for AHB idle */
	for (i = 0; i < timeout; i++) {
		udelay(1);
		grstctl.d32 = read32(&reg->core.grstctl);
		if (grstctl.ahbidle)
			break;
	}
	if (i == timeout)
		die("DWC2 Init error AHB Idle\n");

	/* Restart the Phy Clock */
	write32(&reg->pcgr.pcgcctl, 0x0);
	/* Core soft reset */
	grstctl.csftrst = 1;
	write32(&reg->core.grstctl, grstctl.d32);
	for (i = 0; i < timeout; i++) {
		udelay(1);
		grstctl.d32 = read32(&reg->core.grstctl);
		if (!grstctl.csftrst)
			break;
	}
	if (i == timeout)
		die("DWC2 Init error reset fail\n");

	/* Set 16bit PHY if & Force host mode */
	gusbcfg.d32 = read32(&reg->core.gusbcfg);
	gusbcfg.phyif = 1;
	gusbcfg.forcehstmode = 1;
	gusbcfg.forcedevmode = 0;
	write32(&reg->core.gusbcfg, gusbcfg.d32);
	/* Wait for force host mode effect, it may takes 100ms */
	for (i = 0; i < timeout; i++) {
		udelay(10);
		gintsts.d32 = read32(&reg->core.gintsts);
		if (gintsts.curmod)
			break;
	}
	if (i == timeout)
		die("DWC2 Init error force host mode fail\n");

	/*
	 * Config FIFO
	 * The non-periodic tx fifo and rx fifo share one continuous
	 * piece of IP-internal SRAM.
	 */

	/*
	 * Read total data FIFO depth from HWCFG3
	 * this value is in terms of 32-bit words
	 */
	hwcfg3.d32 = read32(&reg->core.ghwcfg3);
	/*
	 * Reserve 2 spaces for the status entries of received packets
	 * and 2 spaces for bulk and control OUT endpoints. Calculate how
	 * many blocks can be alloted, assume largest packet size is 512.
	 * 16 locations reserved for periodic TX .
	 */
	fifo_blocks = (hwcfg3.dfifodepth - 4 - 16) / (512 / 4);
	tx_blocks = fifo_blocks / 2;

	grxfsiz.rxfdep = (fifo_blocks - tx_blocks) * (512 / 4) + 4;
	write32(&reg->core.grxfsiz, grxfsiz.d32);
	gnptxfsiz.txfstaddr = grxfsiz.rxfdep;
	gnptxfsiz.txfdep = tx_blocks * (512 / 4);
	write32(&reg->core.gnptxfsiz, gnptxfsiz.d32);
	hptxfsiz.txfstaddr = gnptxfsiz.txfstaddr + gnptxfsiz.txfdep;
	hptxfsiz.txfdep = 16;
	write32(&reg->core.hptxfsiz, hptxfsiz.d32);

	/* Init host channels */
	hcintmsk.xfercomp = 1;
	hcintmsk.xacterr = 1;
	hcintmsk.stall = 1;
	hcintmsk.chhltd = 1;
	hcintmsk.bblerr = 1;
	for (i = 0; i < MAX_EPS_CHANNELS; i++)
		write32(&reg->host.hchn[i].hcintmaskn, hcintmsk.d32);

	/* Unmask interrupt and configure DMA mode */
	gahbcfg.glblintrmsk = 1;
	gahbcfg.hbstlen = DMA_BURST_INCR8;
	gahbcfg.dmaen = 1;
	write32(&reg->core.gahbcfg, gahbcfg.d32);

	DWC2_INST(controller)->hprt0 = &reg->host.hprt;

	usb_debug("DWC2 init finished!\n");
}

static void dwc2_shutdown(UsbDevHc *controller)
{
	detach_controller(controller);
	free(DWC2_INST(controller)->dma_buffer);
	free(DWC2_INST(controller));
	free(controller);
}

/* Test root port device connect status */
static int dwc2_disconnected(UsbDevHc *controller)
{
	dwc2_reg_t *reg = DWC2_REG(controller);
	hprt_t hprt;

	hprt.d32 = read32(&reg->host.hprt);
	return !(hprt.prtena && hprt.prtconnsts);
}

/*
 * This function returns the actual transfer length when the transfer succeeded
 * or an error code if the transfer failed
 */
static int
wait_for_complete(UsbEndpoint *ep, uint32_t ch_num)
{
	hcint_t hcint;
	hcchar_t hcchar;
	hctsiz_t hctsiz;
	dwc2_reg_t *reg = DWC2_REG(ep->dev->controller);
	int timeout = 600000; /* time out after 600000 * 5us == 3s */

	/*
	 * TODO: We should take care of up to three times of transfer error
	 * retry here, according to the USB 2.0 spec 4.5.2
	 */
	do {
		udelay(5);
		hcint.d32 = read32(&reg->host.hchn[ch_num].hcintn);
		hctsiz.d32 = read32(&reg->host.hchn[ch_num].hctsizn);

		if (hcint.chhltd) {
			write32(&reg->host.hchn[ch_num].hcintn, hcint.d32);
			if (hcint.xfercomp || hcint.ack)
				return hctsiz.xfersize;
			else if (hcint.nak || hcint.frmovrun)
				return -HCSTAT_NAK;
			else if (hcint.xacterr)
				return -HCSTAT_XFERERR;
			else if (hcint.bblerr)
				return -HCSTAT_BABBLE;
			else if (hcint.stall)
				return -HCSTAT_STALL;
			else if (hcint.nyet)
				return -HCSTAT_NYET;
			else
				return -HCSTAT_UNKNOW;
		}

		if (dwc2_disconnected(ep->dev->controller))
			return -HCSTAT_DISCONNECTED;
	} while (timeout--);
	/* Release the channel when hit timeout condition */
	hcchar.d32 = read32(&reg->host.hchn[ch_num].hccharn);
	if (hcchar.chen) {
		/*
		 * Programming the HCCHARn register with the chdis and
		 * chena bits set to 1 at the same time to disable the
		 * channel and the core will generate a channel halted
		 * interrupt.
		 */
		 hcchar.chdis = 1;
		 write32(&reg->host.hchn[ch_num].hccharn, hcchar.d32);
		 do {
			hcchar.d32 = read32(&reg->host.hchn[ch_num].hccharn);
		 } while (hcchar.chen);

	}

	/* Clear all pending interrupt flags */
	hcint.d32 = ~0;
	write32(&reg->host.hchn[ch_num].hcintn, hcint.d32);

	return -HCSTAT_TIMEOUT;
}

static int
dwc2_do_xfer(UsbEndpoint *ep, int size, int pid, ep_dir_t dir,
	     uint32_t ch_num, uint8_t *data_buf, int *short_pkt)
{
	uint32_t do_copy;
	int ret;
	uint32_t packet_cnt;
	uint32_t packet_size;
	uint32_t transferred = 0;
	uint32_t inpkt_length;
	hctsiz_t hctsiz = { .d32 = 0 };
	hcchar_t hcchar = { .d32 = 0 };
	void *aligned_buf;
	dwc2_reg_t *reg = DWC2_REG(ep->dev->controller);

	packet_size = ep->maxpacketsize;
	packet_cnt = ALIGN_UP(size, packet_size) / packet_size;
	inpkt_length = packet_cnt * packet_size;
	/* At least 1 packet should be programed */
	packet_cnt = (packet_cnt == 0) ? 1 : packet_cnt;

	/*
	 * For an UsbDirIn, this field is the buffer size that the application
	 * has reserved for the transfer. The application should program this
	 * field as integer multiple of the maximum packet size for UsbDirIn
	 * transactions.
	 */
	hctsiz.xfersize = (dir == EPDIR_OUT) ? size : inpkt_length;
	hctsiz.pktcnt = packet_cnt;
	hctsiz.pid = pid;

	hcchar.mps = packet_size;
	hcchar.epnum = ep->endpoint & 0xf;
	hcchar.epdir = dir;
	hcchar.eptype = ep->type;
	hcchar.multicnt = 1;
	hcchar.devaddr = ep->dev->address;
	hcchar.chdis = 0;
	hcchar.chen = 1;
	if (ep->dev->speed == UsbLowSpeed)
		hcchar.lspddev = 1;

	if (size > DMA_SIZE) {
		usb_debug("Transfer too large: %d\n", size);
		return -1;
	}

	/*
	 * Check the buffer address which should be 4-byte aligned and DMA
	 * coherent
	 */
	do_copy = !dma_coherent(data_buf) || ((uintptr_t)data_buf & 0x3);
	aligned_buf = do_copy ? DWC2_INST(ep->dev->controller)->dma_buffer :
		      data_buf;

	if (do_copy && (dir == EPDIR_OUT))
		memcpy(aligned_buf, data_buf, size);

	if (dwc2_disconnected(ep->dev->controller))
		return -HCSTAT_DISCONNECTED;

	write32(&reg->host.hchn[ch_num].hctsizn, hctsiz.d32);
	write32(&reg->host.hchn[ch_num].hcdman,
		(uint32_t)(uintptr_t)aligned_buf);
	write32(&reg->host.hchn[ch_num].hccharn, hcchar.d32);

	ret = wait_for_complete(ep, ch_num);

	if (ret >= 0) {
		/* Calculate actual transferred length */
		transferred = (dir == EPDIR_IN) ? inpkt_length - ret : size;

		if (do_copy && (dir == EPDIR_IN))
			memcpy(data_buf, aligned_buf, transferred);

		if ((short_pkt != NULL) && (dir == EPDIR_IN))
			*short_pkt = (ret > 0) ? 1 : 0;

	}

	/* Save data toggle */
	hctsiz.d32 = read32(&reg->host.hchn[ch_num].hctsizn);
	ep->toggle = hctsiz.pid;

	if (ret < 0) {
		usb_debug("%s Transfer stop code: %x\n", __func__, ret);
		return ret;
	}
	return transferred;
}

static int
dwc2_split_transfer(UsbEndpoint *ep, int size, int pid, ep_dir_t dir,
		    uint32_t ch_num, uint8_t *data_buf, split_info_t *split,
		    int *short_pkt)
{
	dwc2_reg_t *reg = DWC2_REG(ep->dev->controller);
	hfnum_t hfnum;
	hcsplit_t hcsplit = { .d32 = 0 };
	int ret, transferred = 0;

	hcsplit.hubaddr = split->hubaddr;
	hcsplit.prtaddr = split->hubport;
	hcsplit.spltena = 1;
	write32(&reg->host.hchn[ch_num].hcspltn, hcsplit.d32);

	/* Wait for next frame boundary */
	do {
		hfnum.d32 = read32(&reg->host.hfnum);

		if (dwc2_disconnected(ep->dev->controller)) {
			ret = -HCSTAT_DISCONNECTED;
			goto out;
		}
	} while (hfnum.frnum % 8 != 0);

	/* Handle Start-Split */
	ret = dwc2_do_xfer(ep, dir == EPDIR_IN ? 0 : size, pid, dir, ch_num,
			   data_buf, NULL);
	if (ret < 0)
		goto out;

	hcsplit.spltena = 1;
	hcsplit.compsplt = 1;
	write32(&reg->host.hchn[ch_num].hcspltn, hcsplit.d32);
	ep->toggle = pid;

	if (dir == EPDIR_OUT)
		transferred += ret;

	/* Handle Complete-Split */
	do {
		ret = dwc2_do_xfer(ep, dir == EPDIR_OUT ? 0 : size, ep->toggle,
				   dir, ch_num, data_buf, short_pkt);
	} while (ret == -HCSTAT_NYET);

	if (dir == EPDIR_IN)
		transferred += ret;

out:
	/* Clear hcsplit reg */
	hcsplit.spltena = 0;
	hcsplit.compsplt = 0;
	write32(&reg->host.hchn[ch_num].hcspltn, hcsplit.d32);

	if (ret < 0)
		return ret;

	return transferred;
}

static int dwc2_need_split(UsbDev *dev, split_info_t *split)
{
	if (dev->speed == UsbHighSpeed)
		return 0;

	if (usb_closest_usb2_hub(dev, &split->hubaddr, &split->hubport))
		return 0;

	return 1;
}

static int
dwc2_transfer(UsbEndpoint *ep, int size, int pid, ep_dir_t dir, uint32_t ch_num,
	      uint8_t *src, uint8_t skip_nak)
{
	split_info_t split;
	int ret, short_pkt, transferred = 0, timeout = 3000;

	ep->toggle = pid;

	do {
		short_pkt = 0;
		if (dwc2_need_split(ep->dev, &split)) {
nak_retry:
			ret = dwc2_split_transfer(ep, MIN(ep->maxpacketsize,
			      size), ep->toggle, dir, 0, src, &split,
			      &short_pkt);

			/*
			 * dwc2_split_transfer() waits for the next FullSpeed
			 * frame boundary, so we have one try per millisecond.
			 * It's 3s timeout for each split transfer.
			 */
			if (ret == -HCSTAT_NAK && !skip_nak && --timeout) {
				udelay(500);
				goto nak_retry;
			}
		} else {
			ret = dwc2_do_xfer(ep, MIN(DMA_SIZE, size), pid, dir, 0,
			      src, &short_pkt);
		}

		if (ret < 0)
			return ret;

		size -= ret;
		src += ret;
		transferred += ret;

	} while (size > 0 && !short_pkt);

	return transferred;
}

static int
dwc2_bulk(UsbEndpoint *ep, int size, uint8_t *src, int finalize)
{
	ep_dir_t data_dir;

	if (ep->direction == UsbDirIn)
		data_dir = EPDIR_IN;
	else if (ep->direction == UsbDirOut)
		data_dir = EPDIR_OUT;
	else
		return -1;

	return dwc2_transfer(ep, size, ep->toggle, data_dir, 0, src, 0);
}

static int
dwc2_control(UsbDev *dev, UsbDirection dir, int drlen, void *setup,
		   int dalen, uint8_t *src)
{
	int ret = 0;
	ep_dir_t data_dir;
	UsbEndpoint *ep = &dev->endpoints[0];

	if (dir == UsbDirIn)
		data_dir = EPDIR_IN;
	else if (dir == UsbDirOut)
		data_dir = EPDIR_OUT;
	else
		return -1;

	/* Setup Phase */
	if (dwc2_transfer(ep, drlen, PID_SETUP, EPDIR_OUT, 0, setup, 0) < 0)
		return -1;

	/* Data Phase */
	ep->toggle = PID_DATA1;
	if (dalen > 0) {
		ret = dwc2_transfer(ep, dalen, ep->toggle, data_dir, 0, src, 0);
		if (ret < 0)
			return -1;
	}

	/* Status Phase */
	if (dwc2_transfer(ep, 0, PID_DATA1, !data_dir, 0, NULL, 0) < 0)
		return -1;

	return ret;
}

static int
dwc2_intr(UsbEndpoint *ep, int size, uint8_t *src)
{
	ep_dir_t data_dir;

	if (ep->direction == UsbDirIn)
		data_dir = EPDIR_IN;
	else if (ep->direction == UsbDirOut)
		data_dir = EPDIR_OUT;
	else
		return -1;

	return dwc2_transfer(ep, size, ep->toggle, data_dir, 0, src, 1);
}

static uint32_t dwc2_intr_get_timestamp(intr_queue_t *q)
{
	hprt_t hprt;
	hfnum_t hfnum;
	UsbDevHc *controller = q->endp->dev->controller;
	dwc_ctrl_t *dwc2 = DWC2_INST(controller);
	dwc2_reg_t *reg = DWC2_REG(controller);

	hfnum.d32 = read32(&reg->host.hfnum);
	hprt.d32 = read32(dwc2->hprt0);

	/*
	 * hfnum.frnum increments when a new SOF is transmitted on
	 * the USB, and is reset to 0 when it reaches 16'h3FFF
	 */
	switch (hprt.prtspd) {
	case PRTSPD_HIGH:
		/* 8 micro-frame per ms for high-speed */
		return hfnum.frnum / 8;
	case PRTSPD_FULL:
	case PRTSPD_LOW:
	default:
		/* 1 micro-frame per ms for high-speed */
		return hfnum.frnum / 1;
	}
}

/* create and hook-up an intr queue into device schedule */
static void *
dwc2_create_intr_queue(UsbEndpoint *ep, const int reqsize,
		       const int reqcount, const int reqtiming)
{
	intr_queue_t *q = (intr_queue_t *)xzalloc(sizeof(intr_queue_t));

	q->data = dma_memalign(4, reqsize);
	q->endp = ep;
	q->reqsize = reqsize;
	q->reqtiming = reqtiming;

	return q;
}

static void
dwc2_destroy_intr_queue(UsbEndpoint *ep, void *_q)
{
	intr_queue_t *q = (intr_queue_t *)_q;

	free(q->data);
	free(q);
}

/*
 * read one intr-packet from queue, if available. extend the queue for
 * new input. Return NULL if nothing new available.
 * Recommended use: while (data=poll_intr_queue(q)) process(data);
 */
static uint8_t *
dwc2_poll_intr_queue(void *_q)
{
	intr_queue_t *q = (intr_queue_t *)_q;
	int ret = 0;
	uint32_t timestamp = dwc2_intr_get_timestamp(q);

	/*
	 * If hfnum.frnum run overflow it will schedule
	 * an interrupt transfer immediately
	 */
	if (timestamp - q->timestamp < q->reqtiming)
		return NULL;

	q->timestamp = timestamp;

	ret = dwc2_intr(q->endp, q->reqsize, q->data);

	if (ret > 0)
		return q->data;
	else
		return NULL;
}

UsbDevHc *dwc2_init(void *bar)
{
	UsbDevHc *controller = new_controller();
	controller->instance = xzalloc(sizeof(dwc_ctrl_t));

	DWC2_INST(controller)->dma_buffer = dma_malloc(DMA_SIZE);
	if (!DWC2_INST(controller)->dma_buffer) {
		usb_debug("Not enough DMA memory for DWC2 bounce buffer\n");
		goto free_dwc2;
	}

	controller->type = UsbDwc2;
	controller->start = dummy;
	controller->stop = dummy;
	controller->reset = dummy;
	controller->init = dwc2_reinit;
	controller->shutdown = dwc2_shutdown;
	controller->bulk = dwc2_bulk;
	controller->control = dwc2_control;
	controller->set_address = usb_generic_set_address;
	controller->finish_device_config = NULL;
	controller->destroy_device = NULL;
	controller->create_intr_queue = dwc2_create_intr_queue;
	controller->destroy_intr_queue = dwc2_destroy_intr_queue;
	controller->poll_intr_queue = dwc2_poll_intr_queue;
	controller->reg_base = (uintptr_t)bar;
	init_device_entry(controller, 0);

	/* Init controller */
	controller->init(controller);

	/* Setup up root hub */
	controller->devices[0]->controller = controller;
	controller->devices[0]->init = dwc2_rh_init;
	controller->devices[0]->init(controller->devices[0]);
	return controller;

free_dwc2:
	detach_controller(controller);
	free(DWC2_INST(controller));
	free(controller);
	return NULL;
}
