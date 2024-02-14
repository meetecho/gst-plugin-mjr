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
 * SECTION:element-mjrmux
 *
 * FIXME:Describe mjrmux here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesink ! mjrmux ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>

#include <jansson.h>

#include "gstmjrmux.h"
#include "gstmjrutils.h"

/* Info header in the structured recording */
static const gchar *header = "MJR00002";
/* Frame header in the structured recording */
static const gchar *frame_header = "MEET";

enum {
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_SILENT
};

/* Pad templates: we take RTP in and shoot buffers out */
static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE("src",
	GST_PAD_SRC,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS_ANY
);
static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE("sink",
	GST_PAD_SINK,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS("application/x-rtp")
);

#define gst_mjr_mux_parent_class parent_class
	G_DEFINE_TYPE(GstMjrMux, gst_mjr_mux, GST_TYPE_ELEMENT);

GST_ELEMENT_REGISTER_DEFINE(mjrmux, "mjrmux", GST_RANK_NONE,
	GST_TYPE_MJR_MUX);

/* Property setters/getters: currently unused */
static void gst_mjr_mux_set_property(GObject *object,
	guint prop_id, const GValue *value, GParamSpec *pspec);
static void gst_mjr_mux_get_property(GObject *object,
	guint prop_id, GValue *value, GParamSpec *pspec);

/* Pad and chain */
static gboolean gst_mjr_mux_sink_event(GstPad *pad,
	GstObject *parent, GstEvent *event);
static GstFlowReturn gst_mjr_mux_chain(GstPad *pad,
	GstObject *parent, GstBuffer *buf);

/* Initialize the mjrmux's class */
static void gst_mjr_mux_class_init(GstMjrMuxClass *klass) {
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;

	gobject_class = (GObjectClass *)klass;
	gstelement_class = (GstElementClass *)klass;

	gobject_class->set_property = gst_mjr_mux_set_property;
	gobject_class->get_property = gst_mjr_mux_get_property;

	g_object_class_install_property (gobject_class, PROP_SILENT,
		g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
			FALSE, G_PARAM_READWRITE));

	gst_element_class_set_details_simple(gstelement_class,
		"Janus MJR Muxer",
		"Codec/Muxer",
		"Mux RTP packets into an MJR recording",
		"Lorenzo Miniero <lorenzo@meetecho.com>");
	gst_element_class_add_static_pad_template(gstelement_class, &srctemplate);
	gst_element_class_add_static_pad_template(gstelement_class, &sinktemplate);
}

/* Initialize the new element */
static void gst_mjr_mux_init(GstMjrMux *mux) {
	/* Reset private properties: we'll only set them when muxing */
	mux->initialized = FALSE;
	mux->video = FALSE;
	mux->codec = 0;
	mux->created = g_get_real_time();
	/* Setup pads and chain */
	mux->sinkpad = gst_pad_new_from_static_template(&sinktemplate, "sink");
	gst_pad_set_event_function(mux->sinkpad,
		GST_DEBUG_FUNCPTR(gst_mjr_mux_sink_event));
	gst_pad_set_chain_function(mux->sinkpad,
		GST_DEBUG_FUNCPTR(gst_mjr_mux_chain));
	gst_element_add_pad(GST_ELEMENT(mux), mux->sinkpad);
	mux->srcpad = gst_pad_new_from_static_template(&srctemplate, "src");
	gst_pad_use_fixed_caps(mux->srcpad);
	gst_element_add_pad(GST_ELEMENT(mux), mux->srcpad);
}

/* Property setter */
static void gst_mjr_mux_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
	GstMjrMux *mux = GST_MJR_MUX(object);
	/* Set the specified property */
	switch(prop_id) {
		case PROP_SILENT:
			mux->silent = g_value_get_boolean(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}

/* Property getter */
static void gst_mjr_mux_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
	GstMjrMux *mux = GST_MJR_MUX(object);
	/* Get the specified property */
	switch(prop_id) {
		case PROP_SILENT:
			g_value_set_boolean(value, mux->silent);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}

/* Handles sink events */
static gboolean gst_mjr_mux_sink_event(GstPad *pad, GstObject *parent, GstEvent *event) {
	GstMjrMux *mux = GST_MJR_MUX(parent);
	g_print("Received %s event: %" GST_PTR_FORMAT "\n",
		GST_EVENT_TYPE_NAME(event), event);

	/* Process the event */
	gboolean ret = FALSE;
	GST_LOG_OBJECT(mux, "Received %s event: %" GST_PTR_FORMAT,
		GST_EVENT_TYPE_NAME(event), event);
	switch(GST_EVENT_TYPE(event)) {
		case GST_EVENT_CAPS: {
			GstCaps *caps = NULL;
			gst_event_parse_caps(event, &caps);
			/* Check if this is audio or video */
			int i = 0;
			gboolean media_found = FALSE, codec_found = FALSE;
			for(i=0; i<gst_caps_get_size(caps); i++) {
				GstStructure *structure = gst_caps_get_structure(caps, i);
				if(!media_found) {
					const gchar *media = gst_structure_get_string(structure, "media");
					if(media) {
						if(!strcasecmp(media, "video")) {
							media_found = TRUE;
							mux->video = TRUE;
						} else if(!strcasecmp(media, "audio")) {
							media_found = TRUE;
							mux->video = FALSE;
						}
					}
				}
				if(!codec_found) {
					const gchar *codec = gst_structure_get_string(structure, "encoding-name");
					if(codec) {
						gboolean video = FALSE;
						mux->codec = gst_mjr_get_codec(codec, &video);
						if(mux->codec) {
							media_found = TRUE;
							codec_found = TRUE;
						}
					}
				}
			}
			if(!media_found || !codec_found)
				break;
			ret = gst_pad_event_default(pad, parent, event);
			break;
		}
		default:
			ret = gst_pad_event_default(pad, parent, event);
			break;
	}
	/* Done */
	return ret;
}

/* Chain function, where we actually mux RTP packets to MJR buffers */
static GstFlowReturn gst_mjr_mux_chain(GstPad *pad, GstObject *parent, GstBuffer *buf) {
	GstMjrMux *mux = GST_MJR_MUX(parent);
	/* Write to the MJR container */
	GstFlowReturn ret = GST_FLOW_OK;
	if(!mux->silent)
		g_print("[mjrmux] Got buffer of %zu bytes\n", gst_buffer_get_size(buf));
	if(!mux->initialized) {
		/* We still need to create the main header, do it now */
		mux->initialized = TRUE;
		GstBuffer *outbuf = gst_buffer_new_memdup(header, strlen(header));
		GstFlowReturn res = gst_pad_push(mux->srcpad, outbuf);
		/* Create a JSON header */
		json_t *info = json_object();
		/* FIXME Codecs should be configurable in the future */
		const gchar *type = mux->video ? "v" : "a";
		json_object_set_new(info, "t", json_string(type));
		json_object_set_new(info, "c", json_string(gst_mjr_codec_string(mux->codec)));
		json_object_set_new(info, "s", json_integer(mux->created));
		json_object_set_new(info, "u", json_integer(g_get_real_time()));
		gchar *info_text = json_dumps(info, JSON_PRESERVE_ORDER);
		json_decref(info);
		if(info_text == NULL) {
			/* Error generating JSON string */
			GST_ELEMENT_ERROR(mux, STREAM, ENCODE, (NULL), ("Invalid data."));
			gst_buffer_unref(buf);
			return GST_FLOW_ERROR;
		}
		/* First of all, write the size of the JSON string */
		guint16 len = strlen(info_text);
		len = g_htons(len);
		outbuf = gst_buffer_new_memdup(&len, sizeof(len));
		res |= gst_pad_push(mux->srcpad, outbuf);
		/* Now write the JSON string itself */
		outbuf = gst_buffer_new_memdup(info_text, strlen(info_text));
		g_free(info_text);
		res |= gst_pad_push(mux->srcpad, outbuf);
	}
	/* Write the RTP packet to the file, starting from the prefix */
	GstBuffer *outbuf = gst_buffer_new_memdup(frame_header, strlen(frame_header));
	ret |= gst_pad_push(mux->srcpad, outbuf);
	guint32 padding = 0;	/* FIXME We should add the write timestamp too */
	outbuf = gst_buffer_new_memdup(&padding, sizeof(padding));
	ret |= gst_pad_push(mux->srcpad, outbuf);
	/* Write the size of the RTP packet */
	guint16 len = gst_buffer_get_size(buf);
	len = g_htons(len);
	outbuf = gst_buffer_new_memdup(&len, sizeof(len));
	ret |= gst_pad_push(mux->srcpad, outbuf);
	/* Send the buffer along */
	ret |= gst_pad_push(mux->srcpad, buf);
	/* Done */
	return ret;
}

/* Register the eleent in the plugin */
gboolean mjr_mux_register(GstPlugin *plugin) {
	return GST_ELEMENT_REGISTER(mjrmux, plugin);
}
