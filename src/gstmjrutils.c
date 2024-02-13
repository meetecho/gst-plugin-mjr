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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gstmjrutils.h"

/* Helper method to obtain an MJR codec string from a codec name */
int gst_mjr_get_codec(const char *codec, gboolean *video) {
	if(!codec)
		return 0;
	/* Let's try video codecs first */
	if(video)
		*video = TRUE;
	if(!strcasecmp(codec, "vp8"))
		return GST_MJR_VP8;
	else if(!strcasecmp(codec, "vp9"))
		return GST_MJR_VP9;
	else if(!strcasecmp(codec, "h264") || !strcasecmp(codec, "h.264"))
		return GST_MJR_H264;
	else if(!strcasecmp(codec, "h265") || !strcasecmp(codec, "h.265"))
		return GST_MJR_H265;
	else if(!strcasecmp(codec, "av1"))
		return GST_MJR_AV1;
	/* Not video, let's try audio */
	if(video)
		*video = FALSE;
	if(!strcasecmp(codec, "opus"))
		return GST_MJR_OPUS;
	else if(!strcasecmp(codec, "pcmu"))
		return GST_MJR_PCMU;
	else if(!strcasecmp(codec, "pcma"))
		return GST_MJR_PCMA;
	else if(!strcasecmp(codec, "g722"))
		return GST_MJR_G722;
	else if(!strcasecmp(codec, "l16"))
		return GST_MJR_L16;
	else if(!strcasecmp(codec, "l16-48"))
		return GST_MJR_L16_48;
	/* Not found */
	return 0;
}

/* Stringify an MJR codec identifier */
const char *gst_mjr_codec_string(int codec) {
	switch(codec) {
		case GST_MJR_OPUS:
			return "opus";
		case GST_MJR_PCMU:
			return "pcmu";
		case GST_MJR_PCMA:
			return "pcmu";
		case GST_MJR_G722:
			return "g722";
		case GST_MJR_L16:
			return "l16";
		case GST_MJR_L16_48:
			return "l16-48";
		case GST_MJR_VP8:
			return "vp8";
		case GST_MJR_VP9:
			return "vp9";
		case GST_MJR_H264:
			return "h264";
		case GST_MJR_H265:
			return "h265";
		case GST_MJR_AV1:
			return "av1";
		default:
			break;
	}
	return NULL;
}

/* Helper method to get a GStreamer encoding-name from an MJR codec */
const gchar *gst_mjr_get_encoding_name(int codec) {
	switch(codec) {
		case GST_MJR_OPUS:
			return "OPUS";
		case GST_MJR_PCMU:
			return "PCMU";
		case GST_MJR_PCMA:
			return "PCMA";
		case GST_MJR_G722:
			return "G722";
		case GST_MJR_L16:
			return "L16";		/* FIXME */
		case GST_MJR_L16_48:
			return "L16-48";	/* FIXME */
		case GST_MJR_VP8:
			return "VP8";
		case GST_MJR_VP9:
			return "VP9";
		case GST_MJR_H264:
			return "H264";
		case GST_MJR_H265:
			return "H265";
		case GST_MJR_AV1:
			return "AV1";
		default:
			break;
	}
	return NULL;
}

/* Helper method to get a GStreamer clock-rate from an MJR codec */
guint32 gst_mjr_get_clock_rate(int codec) {
	switch(codec) {
		case GST_MJR_OPUS:
			return 48000;
		case GST_MJR_PCMU:
		case GST_MJR_PCMA:
		case GST_MJR_G722:
			return 8000;
		case GST_MJR_L16:
		case GST_MJR_L16_48:
			return 16000;
		case GST_MJR_VP8:
		case GST_MJR_VP9:
		case GST_MJR_H264:
		case GST_MJR_H265:
		case GST_MJR_AV1:
			return 90000;
		default:
			break;
	}
	return 0;
}
