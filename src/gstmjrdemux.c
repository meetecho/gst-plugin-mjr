/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2024 Lorenzo Miniero <lorenzo@meetecho.com>
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

/**
 * SECTION:element-mjrdemux
 *
 * FIXME:Describe mjrdemux here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! mjrdemux ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>

#include <json-glib/json-glib.h>

#include "gstmjrdemux.h"
#include "gstmjrutils.h"

enum {
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_SILENT,
	PROP_SSRC,
	PROP_RANDOM_SSRC
};

/* Pad templates: we take buffers in and shoot RTP out */
static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE("src",
	GST_PAD_SRC,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS("application/x-rtp")
);
static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE("sink",
	GST_PAD_SINK,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS_ANY
);

#define gst_mjr_demux_parent_class parent_class
	G_DEFINE_TYPE(GstMjrDemux, gst_mjr_demux, GST_TYPE_ELEMENT);

GST_ELEMENT_REGISTER_DEFINE(mjrdemux, "mjrdemux", GST_RANK_NONE,
	GST_TYPE_MJR_DEMUX);

/* Property setters/getters: currently unused */
static void gst_mjr_demux_set_property(GObject *object,
	guint prop_id, const GValue *value, GParamSpec *pspec);
static void gst_mjr_demux_get_property(GObject *object,
	guint prop_id, GValue *value, GParamSpec *pspec);

/* Chain function, where we'll process the MJR buffers */
static GstFlowReturn gst_mjr_demux_chain(GstPad *pad,
	GstObject *parent, GstBuffer *buf);

/* Initialize the mjrdemux's class */
static void gst_mjr_demux_class_init(GstMjrDemuxClass *klass) {
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;

	gobject_class = (GObjectClass *)klass;
	gstelement_class = (GstElementClass *)klass;

	gobject_class->set_property = gst_mjr_demux_set_property;
	gobject_class->get_property = gst_mjr_demux_get_property;

	g_object_class_install_property (gobject_class, PROP_SILENT,
		g_param_spec_boolean ("silent", "Silent", "Don't produce verbose output",
			TRUE, G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING));
	g_object_class_install_property (gobject_class, PROP_SSRC,
		g_param_spec_uint("ssrc", "SSRC", "Use a specific SSRC for the outgoing RTP traffic",
			0, G_MAXUINT32, 0, G_PARAM_READWRITE));
	g_object_class_install_property (gobject_class, PROP_RANDOM_SSRC,
		g_param_spec_boolean("randomize-ssrc", "Random SSRC", "Use a random SSRC for the outgoing RTP traffic",
			FALSE, G_PARAM_WRITABLE));

	gst_element_class_set_details_simple(gstelement_class,
		"Janus MJR Demuxer",
		"Codec/Demuxer",
		"Demux MJR recordings to RTP packets",
		"Lorenzo Miniero <lorenzo@meetecho.com>");
	gst_element_class_add_static_pad_template(gstelement_class, &srctemplate);
	gst_element_class_add_static_pad_template(gstelement_class, &sinktemplate);
}

/* Initialize the new element */
static void gst_mjr_demux_init(GstMjrDemux *demux) {
	/* Reset private properties: we'll only set them when demuxing */
	demux->silent = TRUE;
	demux->state = gst_mjr_demux_state_waiting_header;
	demux->legacy = FALSE;
	demux->video = FALSE;
	demux->codec = 0;
	demux->ssrc = 0;
	demux->reading = 0;
	demux->offset = 0;
	demux->pending = 0;
	demux->created = 0;
	demux->written = 0;
	demux->initialized = FALSE;
	demux->last_ts = 0;
	demux->timestamp = 0;
	demux->out_ssrc = 0;
	/* Setup pads and chain */
	demux->sinkpad = gst_pad_new_from_static_template(&sinktemplate, "sink");
	gst_pad_set_chain_function(demux->sinkpad,
		GST_DEBUG_FUNCPTR(gst_mjr_demux_chain));
	gst_element_add_pad(GST_ELEMENT(demux), demux->sinkpad);
	demux->srcpad = gst_pad_new_from_static_template(&srctemplate, "src");
	gst_element_add_pad(GST_ELEMENT(demux), demux->srcpad);
}

/* Property setter */
static void gst_mjr_demux_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
	GstMjrDemux *demux = GST_MJR_DEMUX(object);
	/* Set the specified property */
	switch(prop_id) {
		case PROP_SILENT:
			demux->silent = g_value_get_boolean(value);
			break;
		case PROP_SSRC:
			demux->out_ssrc = g_value_get_uint(value);
			break;
		case PROP_RANDOM_SSRC:
			demux->out_ssrc = g_random_int();
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}

/* Property getter */
static void gst_mjr_demux_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
	GstMjrDemux *demux = GST_MJR_DEMUX(object);
	/* Get the specified property */
	switch(prop_id) {
		case PROP_SILENT:
			g_value_set_boolean(value, demux->silent);
			break;
		case PROP_SSRC:
			g_value_set_uint(value, demux->out_ssrc);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}

/* Chain function, where we actually demux buffers to RTP packets */
static GstFlowReturn gst_mjr_demux_chain(GstPad *pad, GstObject *parent, GstBuffer *buf) {
	GstMjrDemux *demux = GST_MJR_DEMUX(parent);
	/* Process the incoming buffer */
	GstFlowReturn ret = GST_FLOW_OK;
	if(!demux->silent)
		g_print("[mjrdemux] Got buffer of %zu bytes\n", gst_buffer_get_size(buf));
	gsize buf_offset = 0;
	guint16 len = 0;
	gst_mjr_rtp *rtp = NULL;
	while(gst_buffer_get_size(buf) > 0) {
		if(demux->state == gst_mjr_demux_state_waiting_header) {
			/* We've just started, and are waiting for the MJR header */
			demux->state = gst_mjr_demux_state_reading_header;
			demux->reading = 8;		/* The MJR header is 8 bytes */
			demux->offset = 0;
			demux->pending = demux->reading;
		}
		/* We're currently reading the MJR header */
		if(!demux->silent) {
			g_print("[mjrdemux] Reading %" G_GSIZE_FORMAT " bytes at offset %" G_GSIZE_FORMAT " to position %" G_GSIZE_FORMAT "\n",
				demux->pending, buf_offset, demux->offset);
		}
		gsize extracted = gst_buffer_extract(buf, buf_offset, demux->buffer + demux->offset, demux->pending);
		buf_offset += extracted;
		demux->offset += extracted;
		demux->pending -= extracted;
		if(demux->pending > 0) {
			/* We haven't read all we need, continue later */
			break;
		}
		if(demux->state == gst_mjr_demux_state_reading_header) {
			/* If we got here we have the first part of the header, verify it's an MJR */
			if(demux->buffer[0] != 'M') {
				/* Not an MJR file */
				GST_ELEMENT_ERROR(demux, STREAM, DECODE, (NULL), ("Not an MJR file."));
				ret = GST_FLOW_ERROR;
				break;
			}
			if(demux->buffer[1] == 'J' && demux->buffer[2] == 'R' && demux->buffer[3] == '0' && demux->buffer[4] == '0' &&
					demux->buffer[5] == '0' && demux->buffer[6] == '0' && demux->buffer[7] == '2') {
				/* New format (MJR00002) */
				if(!demux->silent)
					g_print("[mjrdemux] New MJR format\n");
			} else if(demux->buffer[1] == 'E' && demux->buffer[2] == 'E' && demux->buffer[3] == 'T' && demux->buffer[4] == 'E' &&
					demux->buffer[5] == 'C' && demux->buffer[6] == 'H' && demux->buffer[7] == 'O') {
				/* Legacy format (MEETECHO) */
				if(!demux->silent)
					g_print("[mjrdemux] Legacy MJR format\n");
				demux->legacy = TRUE;
			} else {
				/* Not an MJR file */
				GST_ELEMENT_ERROR(demux, STREAM, DECODE, (NULL), ("Not an MJR file, or unsupported version."));
				ret = GST_FLOW_ERROR;
				break;
			}
			/* Done, change state */
			demux->state = gst_mjr_demux_state_waiting_json;
			/* The MJR JSON header is prefixed by a 2 bytes length */
			demux->reading = 2;
			demux->offset = 0;
			demux->pending = demux->reading;
			continue;
		} else if(demux->state == gst_mjr_demux_state_waiting_json) {
			/* If we got here we have the length of the JSON header */
			memcpy(&len, demux->buffer, sizeof(len));
			len = g_ntohs(len);
			if(demux->legacy && len != 5) {
				/* Legacy MJR files always have only either "audio" or "video" */
				GST_ELEMENT_ERROR(demux, STREAM, DECODE, (NULL), ("Invalid header length for legacy MJR file."));
				ret = GST_FLOW_ERROR;
				break;
			} else if(len > 1500) {
				/* Too big */
				GST_ELEMENT_ERROR(demux, STREAM, DECODE, (NULL), ("Invalid header length. (%" G_GUINT16_FORMAT ")", len));
				ret = GST_FLOW_ERROR;
				break;
			}
			/* Done, change state */
			demux->state = gst_mjr_demux_state_reading_json;
			/* Read as many bytes as we were told to expect */
			demux->reading = len;
			demux->offset = 0;
			demux->pending = demux->reading;
			continue;
		} else if(demux->state == gst_mjr_demux_state_reading_json) {
			/* Check if this is a legacy or new recording */
			if(demux->legacy) {
				/* Just check if this is audio or video */
				if(demux->buffer[0] == 'a') {
					/* Audio, so Opus in legacy MJR format */
					if(!demux->silent)
						g_print("[mjrdemux] Audio MJR file (legacy, assuming opus)\n");
					demux->video = FALSE;
					demux->codec = GST_MJR_OPUS;
				} else if(demux->buffer[0] == 'v') {
					/* Video, so VP8 in legacy MJR format */
					if(!demux->silent)
						g_print("[mjrdemux] Video MJR file (legacy, assuming VP8)\n");
					demux->video = TRUE;
					demux->codec = GST_MJR_VP8;
				} else {
					/* Unsupported format */
					GST_ELEMENT_ERROR(demux, STREAM, DECODE, (NULL), ("Unsupported media format."));
					ret = GST_FLOW_ERROR;
					break;
				}
			} else {
				/* Parse the header for info on codecs, start times, etc. */
				demux->buffer[demux->reading] = '\0';
				if(!demux->silent)
					g_print("[mjrdemux] JSON header: %s\n", demux->buffer);
				GError *error = NULL;
				JsonParser *parser = json_parser_new();
				if(!json_parser_load_from_data(parser, demux->buffer, -1, &error)) {
					GST_ELEMENT_ERROR(demux, STREAM, DECODE, (NULL), ("Invalid JSON header."));
					ret = GST_FLOW_ERROR;
					g_object_unref(parser);
					break;
				}
				JsonReader *reader = json_reader_new(json_parser_get_root(parser));
				json_reader_read_member(reader, "t");
				const gchar *t = json_reader_get_string_value(reader);
				json_reader_end_member(reader);
				json_reader_read_member(reader, "c");
				const gchar *c = json_reader_get_string_value(reader);
				json_reader_end_member(reader);
				json_reader_read_member(reader, "s");
				guint64 s = json_reader_get_int_value(reader);
				json_reader_end_member(reader);
				json_reader_read_member(reader, "u");
				guint64 u = json_reader_get_int_value(reader);
				json_reader_end_member(reader);
				if(!t || !c || !s || !u) {
					GST_ELEMENT_ERROR(demux, STREAM, DECODE, (NULL), ("Invalid JSON header."));
					ret = GST_FLOW_ERROR;
					g_object_unref(reader);
					g_object_unref(parser);
					break;
				}
				if(!strcasecmp(t, "v")) {
					demux->video = TRUE;
				} else if(!strcasecmp(t, "a")) {
					demux->video = FALSE;
				} else if(!strcasecmp(t, "d")) {
					GST_ELEMENT_ERROR(demux, STREAM, DECODE, (NULL), ("Unsupported media format."));
					ret = GST_FLOW_ERROR;
					g_object_unref(reader);
					g_object_unref(parser);
					break;
				}
				demux->codec = gst_mjr_get_codec(c, &demux->video);
				if(!demux->codec) {
					GST_ELEMENT_ERROR(demux, STREAM, DECODE, (NULL), ("Unsupported codec (%s).", c));
					ret = GST_FLOW_ERROR;
					g_object_unref(reader);
					g_object_unref(parser);
					break;
				}
				demux->created = s;
				demux->written = u;
				g_object_unref(reader);
				g_object_unref(parser);
			}
			/* Done, change state */
			demux->state = gst_mjr_demux_state_waiting_packet;
			/* RTP packets are prefixed by a 8 bytes payload and a 2 bytes length header */
			demux->reading = 10;
			demux->offset = 0;
			demux->pending = demux->reading;
			continue;
		} else if(demux->state == gst_mjr_demux_state_waiting_packet) {
			/* If we got here we have the prefix and the length of the RTP packet */
			if(demux->buffer[0] != 'M' || demux->buffer[1] != 'E' || demux->buffer[2] != 'E' || demux->buffer[3] != 'T') {
				/* Not what we were expecting */
				GST_ELEMENT_ERROR(demux, STREAM, DECODE, (NULL), ("Invalid data."));
				ret = GST_FLOW_ERROR;
				break;
			}
			/* FIXME We could read the "Received time" from there too,
			 * but we ignore it for now, and move right to the length */
			memcpy(&len, demux->buffer + 8, sizeof(len));
			len = g_ntohs(len);
			if(len > 1500) {
				/* Too big */
				GST_ELEMENT_ERROR(demux, STREAM, DECODE, (NULL), ("Invalid packet length. (%" G_GUINT16_FORMAT ")", len));
				ret = GST_FLOW_ERROR;
				break;
			}
			/* Done, change state */
			demux->state = gst_mjr_demux_state_reading_packet;
			/* Read as many bytes as we were told to expect */
			demux->reading = len;
			demux->offset = 0;
			demux->pending = demux->reading;
			continue;
		} else if(demux->state == gst_mjr_demux_state_reading_packet) {
			/* We got an RTP packet */
			rtp = (gst_mjr_rtp *)demux->buffer;
			if(!demux->silent) {
				g_print("[mjrdemux][RTP] seq=%5" G_GUINT16_FORMAT ", ts=%10" G_GUINT32_FORMAT "\n",
					g_ntohs(rtp->seq_number), g_ntohl(rtp->timestamp));
			}
			/* Process the RTP packet to create a buffer to pass along */
			if(demux->ssrc == 0)
				demux->ssrc = g_ntohl(rtp->ssrc);
			if(g_ntohl(rtp->ssrc) != demux->ssrc) {
				/* Ignore packet */
			} else {
				/* Turn timestamp in timing information */
				if(!demux->initialized) {
					demux->initialized = TRUE;
					demux->last_ts = g_ntohl(rtp->timestamp);
					/* Update the caps on the source pad */
					GstCaps *newcaps = gst_caps_new_simple("application/x-rtp",
						"media", G_TYPE_STRING, (demux->video ? "video" : "audio"),
						"encoding-name", G_TYPE_STRING, gst_mjr_get_encoding_name(demux->codec),
						"clock-rate", G_TYPE_INT, gst_mjr_get_clock_rate(demux->codec),
						"payload", G_TYPE_INT, rtp->type,
						NULL);
					gboolean res = gst_pad_set_caps(demux->srcpad, newcaps);
					char *caps_str = gst_caps_to_string(newcaps);
					g_print("[mjrdemux] Caps %s set to '%s'\n", (res ? "successfully" : "NOT"), caps_str);
					g_free(caps_str);
					/* Notify new segment */
					GstSegment segment;
					gst_segment_init(&segment, GST_FORMAT_TIME);
					GstEvent *event = gst_event_new_segment(&segment);
					gst_pad_push_event(demux->srcpad, event);
				}
				double diff = (double)(g_ntohl(rtp->timestamp) - demux->last_ts)/(double)gst_mjr_get_clock_rate(demux->codec);
				demux->timestamp += diff * G_USEC_PER_SEC * 1000;
				if(!demux->silent)
					g_print("[mjrdemux][RTP] Computed timestamp: %" G_GUINT64_FORMAT "\n", demux->timestamp);
				demux->last_ts = g_ntohl(rtp->timestamp);
				/* Check if we need to overwrite the SSRC */
				if(demux->out_ssrc)
					rtp->ssrc = g_htonl(demux->out_ssrc);
				/* Create a buffer and pass it along the pad */
				GstBuffer *outbuf = gst_buffer_new_memdup(demux->buffer, demux->reading);
				GST_BUFFER_TIMESTAMP(outbuf) = demux->timestamp;
				GstFlowReturn res = gst_pad_push(demux->srcpad, outbuf);
				if(res != GST_FLOW_OK) {
					GST_ELEMENT_ERROR(demux, CORE, PAD, (NULL),
						("Error pushing buffer to pad (%d)", res));
					ret = res;
					break;
				}
			}
			/* Done, change state */
			demux->state = gst_mjr_demux_state_waiting_packet;
			/* Next packet */
			demux->reading = 10;
			demux->offset = 0;
			demux->pending = demux->reading;
			continue;
		}
	}

	gst_buffer_unref(buf);
	return ret;
}

/* Register the eleent in the plugin */
gboolean mjr_demux_register(GstPlugin *plugin) {
	return GST_ELEMENT_REGISTER(mjrdemux, plugin);
}
