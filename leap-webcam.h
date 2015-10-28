/*
 * stk-webcam.h : Driver for Syntek 1125 USB webcam controller
 *
 * Copyright (C) 2006 Nicolas VIVIEN
 * Copyright 2007-2008 Jaime Velasco Juan <jsagarribay@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef LEAPMOTION_H
#define LEAPMOTION_H

#include <linux/usb.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-common.h>

#define DRIVER_VERSION		"v0.0.1"
#define DRIVER_VERSION_NUM	0x000001

#define MAX_ISO_BUFS		3
#define ISO_FRAMES_PER_DESC	16
#define ISO_MAX_FRAME_SIZE	3 * 1024
#define ISO_BUFFER_SIZE		(ISO_FRAMES_PER_DESC * ISO_MAX_FRAME_SIZE)


#define PREFIX				"leapmotion: "
#define LEAP_INFO(str, args...)		printk(KERN_INFO PREFIX str, ##args)
#define LEAP_ERROR(str, args...)		printk(KERN_ERR PREFIX str, ##args)
#define LEAP_WARNING(str, args...)	printk(KERN_WARNING PREFIX str, ##args)

struct leap_iso_buf {
	void *data;
	int length;
	int read;
	struct urb *urb;
};

/* Streaming IO buffers */
struct leap_sio_buffer {
	struct v4l2_buffer v4lbuf;
	char *buffer;
	int mapcount;
	struct leap_camera *dev;
	struct list_head list;
};

/* it SHOULD have higher res modes, but I cannot seem to get them to work */ 
enum leap_mode {MODE_VGA, MODE_SXGA, MODE_CIF, MODE_QVGA, MODE_QCIF};

struct leap_video {
	enum leap_mode mode;
	__u32 palette;
	int hflip;
	int vflip;
};

enum leap_status {
	S_PRESENT = 1,
	S_INITIALISED = 2,
	S_MEMALLOCD = 4,
	S_STREAMING = 8,
};
#define is_present(dev)		((dev)->status & S_PRESENT)
#define is_initialised(dev)	((dev)->status & S_INITIALISED)
#define is_streaming(dev)	((dev)->status & S_STREAMING)
#define is_memallocd(dev)	((dev)->status & S_MEMALLOCD)
#define set_present(dev)	((dev)->status = S_PRESENT)
#define unset_present(dev)	((dev)->status &= \
~(S_PRESENT|S_INITIALISED|S_STREAMING))
#define set_initialised(dev)	((dev)->status |= S_INITIALISED)
#define unset_initialised(dev)	((dev)->status &= ~S_INITIALISED)
#define set_memallocd(dev)	((dev)->status |= S_MEMALLOCD)
#define unset_memallocd(dev)	((dev)->status &= ~S_MEMALLOCD)
#define set_streaming(dev)	((dev)->status |= S_STREAMING)
#define unset_streaming(dev)	((dev)->status &= ~S_STREAMING)

struct regval {
	unsigned reg;
	unsigned val;
};

struct leap_camera {
	struct v4l2_device v4l2_dev;
	struct v4l2_ctrl_handler hdl;
	struct video_device vdev;
	struct usb_device *udev;
	struct usb_interface *interface;
	int webcam_model;
	struct file *owner;
	struct mutex lock;
	int first_init;
	
	u8 isoc_ep;
	
	/* Not sure if this is right */
	/* I'm not so sure either, but I trust you */
	atomic_t urbs_used;
	
	struct leap_video vsettings;
	
	enum leap_status status;
	
	spinlock_t spinlock;
	wait_queue_head_t wait_frame;
	
	struct leap_iso_buf *isobufs;
	
	int frame_size;
	/* Streaming buffers */
	int reading;
	unsigned int n_sbufs;
	struct leap_sio_buffer *sio_bufs;
	struct list_head sio_avail;
	struct list_head sio_full;
	unsigned sequence;
};

#define vdev_to_camera(d) container_of(d, struct leap_camera, vdev)

int leap_camera_write_reg(struct leap_camera *, u16, u8);
int leap_camera_read_reg(struct leap_camera *, u16, int *);

#endif
