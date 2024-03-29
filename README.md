GStreamer Janus MJR Plugin
==========================

This is an implementation of a [GStreamer](https://gstreamer.freedesktop.org/) muxer and demuxer plugin for [Janus](https://github.com/meetecho/janus-gateway/) MJR recordings, developed by [Meetecho](http://www.meetecho.com).

# Meetecho Janus Recordings (MJR)

Janus supports recording out of the box, and it does so by saving RTP streams to a structured container, called an MJR file, whose format is documented [here](https://janus.conf.meetecho.com/docs/recordings). Each individual stream is stored in a separate MJR file, which makes it lightweight to record streams. It's then up to a post-processing phase to extract the media frames from the RTP packets stored in the container, and optionally mux/mix them thereafter.

Janus provides a command line application called `janus-pp-rec` to extract frames to a playable container, plus a couple of separate tools to convert MJR recordings to/from pcap files. The purpose of this repo is providing a muxer and a demuxer for the MJR format that can be used within the context of GStreamer pipelines, e.g., for the purpose of replicating the `janus-pp-rec` functionality, watch recordings, replay RTP sessions, create MJR sessions within GStreamer applications and more.

# GStreamer MJR plugin

The GStreamer MJR plugin provides two different elements:

* `mjrdemux`: a Janus MJR Demuxer;
* `mjrmux`: a Janus MJR Muxer.

The `mjrdemux` supports the following properties:

* `silent` (boolean): Don't produce verbose output (`true` by default);
* `ssrc` (unsigned int): Use a specific SSRC for the outgoing RTP traffic (by default the demuxer just uses the same SSRC used in the MJR file);
* `randomize-ssrc` (boolean): Use a random SSRC for the outgoing RTP traffic (by default the demuxer just uses the same SSRC used in the MJR file).

The `mjrmux` supports the following properties:

* `silent` (boolean): Don't produce verbose output (`true` by default).

## Building the plugin

To build the plugin, you'll need to install the development libraries of GStreamer and `json-glib`, plus `meson` and `ninja` for building it:

	meson builddir
	ninja -C builddir/

If successful, a `libgstmjr.so` library will be created in `builddir`, which you can inspect with `gst-inspect-1.0`:

```
[lminiero@lminiero gst-plugin-mjr]$ gst-inspect-1.0 builddir/libgstmjr.so
Plugin Details:
  Name                     mjr
  Description              Janus MJR Plugin
  Filename                 builddir/libgstmjr.so
  Version                  0.0.1
  License                  LGPL
  Source module            gst-plugin-mjr
  Binary package           gst-plugin-mjr
  Origin URL               https://github.com/meetecho/gst-plugin-mjr

  mjrdemux: Janus MJR Demuxer
  mjrmux: Janus MJR Muxer

  2 features:
  +-- 2 elements
```

Copying that library to the GStreamer plugins folder (e.g., `/usr/lib64/gstreamer-1.0/`) should make it usable to all applications:

	gst-inspect-1.0 mjr

The available elements (`mjrdemux` and `mjrmux`) can also be inspected, for more info on capabilities and available properties.

## Testing the muxer

The `mjrmux` element is quite simple, since it just expects RTP packets on the way in, and will write them in the MJR format. As such, it will typically be used in conjunction with a `filesink` to write the MJR to file. Notice that it needs access to the RTP caps in order to figure out what to write in the header, e.g.:

	gst-launch-1.0 autovideosrc ! videoconvert ! vp8enc ! rtpvp8pay ! \
		mjrmux ! filesink location=test.mjr

or:

	gst-launch-1.0 udpsrc port=5002 ! \
		"application/x-rtp, media=audio, encoding-name=OPUS" ! \
		mjrmux ! filesink location=test.mjr

This should produce MJR files compatible with `janus-pp-rec` (you can use the `-p` flag with that tool to validate them).

## Testing the demuxer

The `mjrdemux` element is a bit more complex, since it takes MJR buffers in, and shoots out RTP streams. Considering MJR files may contain different kind of media, depending on the original encoding, caps will be generated automatically, which should in theory allow dynamic elements to adapt automatically.

This is an example of the demuxer being used to extract the media frames to a playable file:

	gst-launch-1.0 filesrc location=rec-sample-video.mjr ! \
		mjrdemux ! rtpvp8depay ! webmmux ! filesink location=test.webm

This is basically functionally equivalent to the post-processing operations provided by `janus-pp-rec`. This other example, instead, is an example of how to play the contents of an MJR file:

	gst-launch-1.0 filesrc location=rec-sample-video.mjr ! \
		mjrdemux ! rtpvp8depay ! vp8dec ! videoconvert ! autovideosink

This snippet presents an example of how to replay an RTP session captures in an MJR file via RTP again:

	gst-launch-1.0 filesrc location=rec-sample-video.mjr ! \
		mjrdemux ! udpsink host=127.0.0.1 port=5002

As anticipated, MJR files only contain a single stream, so to put together, for instance, an audio and video MJR belonging to the same session, you'll need different demuxer instances:

	gst-launch-1.0 webmmux name=m ! filesink location=test.webm \
		filesrc location=rec-sample-audio.mjr ! mjrdemux ! \
			rtpopusdepay ! m. \
		filesrc location=rec-sample-video.mjr ! mjrdemux ! \
			rtpvp8depay ! m.

Same applies to a synchronized playback of audio and video MJRs:

	gst-launch-1.0 \
		filesrc location=rec-sample-audio.mjr ! mjrdemux ! \
			rtpopusdepay ! opusdec ! audioconvert ! autoaudiosink \
		filesrc location=rec-sample-video.mjr ! mjrdemux ! \
			rtpvp8depay ! vp8dec ! videoconvert ! autovideosink

and a synchronized replay:

	gst-launch-1.0 \
		filesrc location=rec-sample-audio.mjr ! mjrdemux ! \
			udpsink host=127.0.0.1 port=5002 \
		filesrc location=rec-sample-video.mjr ! mjrdemux ! \
			udpsink host=127.0.0.1 port=5004

# Known limitations

This is just a first proof-of-concept version of the MJR plugin, and as such it has a set of known limitations that will hopefully be addressed:

* The "RTP timestamp to Buffer timestamp" conversion is at the moment very barebones, and may not work as expected when dealing with wrap-arounds of RTP timestamps.
* The potential gap between `s` (started/created) and `u` (first written/updated) in the MJR JSON header is ignored by `mjrdemux`, at the moment, which means any potential silence or emptyness that should be "rendered" accordingly will not be implemented by the plugin. This may cause desync issues in some audio/video muxing, as frames may be presented sooner than they should.
* Unlike `janus-pp-rec`, `mjrdemux` doesn't attempt to reorder packets before handling them, but simply processes them as they're read and sets a timestamp on the buffer accordingly. This means that it's up to other plugins in the GStreamer pipeline to deal with potentially out of order packets (`rtpjitterbuffer`?) in order to avoid writing or presenting broken frames.
* Neither `mjrmux` nor `mjrdemux` do anything with RTP extensions, at the moment, as far as signalling is concerned.
* Related to the above, `mjrdemux` doesn't currently provide any looping functionality, which may be helpful in some contexts: the RTP context should be updated as part of the process, though, in order to avoid discontinuities in the resulting RTP stream.
* Apparently, `oggmux` doesn't work when fed by an `rtpopusdepay` element, which means that, unlike `janus-pp-rec`, `mjrdemux` can't be used to extract an Opus MJR to an `.opus` file, unless you also transcode in the middle. That said, this is an `oggmux` limitation, and not something we can fix in `mjrdemux` (unless we somehow figure out what it is that it expects exactly).
* More in general, I've performed very little testing and involving a limited set of plugins: ideally testing with `rtpbin` and/or `decodebin`/`playbin` should be performed too, especially to verify whether or not the dynamic caps set by, e.g., `mjrdemux` are doing their job. I plan to make tests with [Simple WHIP Client](https://github.com/meetecho/simple-whip-client) as well, which is GStreamer based.

# Feedback welcome!

Any thought, feedback or (hopefully not!) insult is welcome!
