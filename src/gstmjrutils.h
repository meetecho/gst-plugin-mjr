/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2020 Niels De Graef <niels.degraef@gmail.com>
 * Copyright (C) 2024 Lorenzo Miniero <<user@hostname.org>>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GST_MJR_UTILS_H__
#define __GST_MJR_UTILS_H__

#include <gst/gst.h>

#if defined(__MACH__) || defined(__FreeBSD__)
#include <machine/endian.h>
#define __BYTE_ORDER BYTE_ORDER
#define __BIG_ENDIAN BIG_ENDIAN
#define __LITTLE_ENDIAN LITTLE_ENDIAN
#else
#include <endian.h>
#endif

/* RTP header */
typedef struct gst_mjr_rtp {
#if __BYTE_ORDER == __BIG_ENDIAN
	guint16 version:2;
	guint16 padding:1;
	guint16 extension:1;
	guint16 csrccount:4;
	guint16 markerbit:1;
	guint16 type:7;
#elif __BYTE_ORDER == __LITTLE_ENDIAN
	guint16 csrccount:4;
	guint16 extension:1;
	guint16 padding:1;
	guint16 version:2;
	guint16 type:7;
	guint16 markerbit:1;
#endif
	guint16 seq_number;
	guint32 timestamp;
	guint32 ssrc;
	guint32 csrc[16];
} gst_mjr_rtp;

/* Supported audio codecs */
#define GST_MJR_OPUS	1
#define GST_MJR_PCMU	2
#define GST_MJR_PCMA	3
#define GST_MJR_G722	4
#define GST_MJR_L16		5
#define GST_MJR_L16_48	6

/* Supported video codecs */
#define GST_MJR_VP8		11
#define GST_MJR_VP9		12
#define GST_MJR_H264	13
#define GST_MJR_H265	14
#define GST_MJR_AV1		15

/* Helper method to obtain an MJR codec from a codec name */
int gst_mjr_get_codec(const char *codec, gboolean *video);
/* Stringify an MJR codec identifier */
const char *gst_mjr_codec_string(int codec);
/* Helper method to get a GStreamer encoding-name from an MJR codec */
const gchar *gst_mjr_get_encoding_name(int codec);
/* Helper method to get a GStreamer clock-rate from an MJR codec */
guint32 gst_mjr_get_clock_rate(int codec);

#endif /* __GST_MJR_UTILS_H__ */
