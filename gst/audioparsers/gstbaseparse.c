/* GStreamer
 * Copyright (C) 2008 Nokia Corporation. All rights reserved.
 *   Contact: Stefan Kost <stefan.kost@nokia.com>
 * Copyright (C) 2008 Sebastian Dröge <sebastian.droege@collabora.co.uk>.
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
 * SECTION:gstbaseparse
 * @short_description: Base class for stream parsers
 * @see_also: #GstBaseTransform
 *
 * This base class is for parser elements that process data and splits it 
 * into separate audio/video/whatever frames.
 *
 * It provides for:
 * <itemizedlist>
 *   <listitem><para>One sinkpad and one srcpad</para></listitem>
 *   <listitem><para>Handles state changes</para></listitem>
 *   <listitem><para>Does flushing</para></listitem>
 *   <listitem><para>Push mode</para></listitem>
 *   <listitem><para>Pull mode</para></listitem>
 *   <listitem><para>Handles events (NEWSEGMENT/EOS/FLUSH)</para></listitem>
 *   <listitem><para>Handles seeking in both modes</para></listitem>
 *   <listitem><para>
 *        Handles POSITION/DURATION/SEEKING/FORMAT/CONVERT queries
 *   </para></listitem>
 * </itemizedlist>
 *
 * The purpose of this base class is to provide a basic functionality of
 * a parser and share a lot of rather complex code.
 *
 * Description of the parsing mechanism:
 * <orderedlist>
 * <listitem>
 *   <itemizedlist><title>Set-up phase</title>
 *   <listitem><para>
 *     GstBaseParse class calls @set_sink_caps to inform the subclass about
 *     incoming sinkpad caps. Subclass should set the srcpad caps accordingly.
 *   </para></listitem>
 *   <listitem><para>
 *     GstBaseParse calls @start to inform subclass that data processing is
 *     about to start now.
 *   </para></listitem>
 *   <listitem><para>
 *      At least in this point subclass needs to tell the GstBaseParse class
 *      how big data chunks it wants to receive (min_frame_size). It can do 
 *      this with @gst_base_parse_set_min_frame_size.
 *   </para></listitem>
 *   <listitem><para>
 *      GstBaseParse class sets up appropriate data passing mode (pull/push)
 *      and starts to process the data.
 *   </para></listitem>
 *   </itemizedlist>
 * </listitem>
 * <listitem>
 *   <itemizedlist>
 *   <title>Parsing phase</title>
 *     <listitem><para>
 *       GstBaseParse gathers at least min_frame_size bytes of data either 
 *       by pulling it from upstream or collecting buffers into internal
 *       #GstAdapter.
 *     </para></listitem>
 *     <listitem><para>
 *       A buffer of min_frame_size bytes is passed to subclass with
 *       @check_valid_frame. Subclass checks the contents and returns TRUE
 *       if the buffer contains a valid frame. It also needs to set the
 *       @framesize according to the detected frame size. If buffer didn't
 *       contain a valid frame, this call must return FALSE and optionally
 *       set the @skipsize value to inform base class that how many bytes
 *       it needs to skip in order to find a valid frame. The passed buffer
 *       is read-only.  Note that @check_valid_frame might receive any small
 *       amount of input data when leftover data is being drained (e.g. at EOS).
 *     </para></listitem>
 *     <listitem><para>
 *       After valid frame is found, it will be passed again to subclass with
 *       @parse_frame call. Now subclass is responsible for parsing the
 *       frame contents and setting the buffer timestamp, duration and caps.
 *     </para></listitem>
 *     <listitem><para>
 *       Finally the buffer can be pushed downstream and parsing loop starts
 *       over again.
 *     </para></listitem>
 *     <listitem><para>
 *       During the parsing process GstBaseParseClass will handle both srcpad and
 *       sinkpad events. They will be passed to subclass if @event or
 *       @src_event callbacks have been provided.
 *     </para></listitem>
 *   </itemizedlist>
 * </listitem>
 * <listitem>
 *   <itemizedlist><title>Shutdown phase</title>
 *   <listitem><para>
 *     GstBaseParse class calls @stop to inform the subclass that data
 *     parsing will be stopped.
 *   </para></listitem>
 *   </itemizedlist>
 * </listitem>
 * </orderedlist>
 *
 * Subclass is responsible for providing pad template caps for
 * source and sink pads. The pads need to be named "sink" and "src". It also 
 * needs to set the fixed caps on srcpad, when the format is ensured (e.g. 
 * when base class calls subclass' @set_sink_caps function).
 *
 * This base class uses GST_FORMAT_DEFAULT as a meaning of frames. So,
 * subclass conversion routine needs to know that conversion from
 * GST_FORMAT_TIME to GST_FORMAT_DEFAULT must return the
 * frame number that can be found from the given byte position.
 *
 * GstBaseParse uses subclasses conversion methods also for seeking. If
 * subclass doesn't provide @convert function, seeking will get disabled.
 *
 * Subclass @start and @stop functions will be called to inform the beginning
 * and end of data processing.
 *
 * Things that subclass need to take care of:
 * <itemizedlist>
 *   <listitem><para>Provide pad templates</para></listitem>
 *   <listitem><para>
 *      Fixate the source pad caps when appropriate
 *   </para></listitem>
 *   <listitem><para>
 *      Inform base class how big data chunks should be retrieved. This is
 *      done with @gst_base_parse_set_min_frame_size function.
 *   </para></listitem>
 *   <listitem><para>
 *      Examine data chunks passed to subclass with @check_valid_frame
 *      and tell if they contain a valid frame
 *   </para></listitem>
 *   <listitem><para>
 *      Set the caps and timestamp to frame that is passed to subclass with
 *      @parse_frame function.
 *   </para></listitem>
 *   <listitem><para>Provide conversion functions</para></listitem>
 *   <listitem><para>
 *      Update the duration information with @gst_base_parse_set_duration
 *   </para></listitem>
 *   <listitem><para>
 *      Alternatively, parsing (or specs) might yield a frames per seconds rate
 *      which can be provided to GstBaseParse to enable it to cater for
 *      buffer time metadata (which will be taken from upstream as much as possible).
 *      Internally keeping track of frames and respective
 *      sizes that have been pushed provides GstBaseParse with a bytes per frame
 *      rate.  A default @convert (used if not overriden) will then use these
 *      rates to perform obvious conversions.  These rates are also used to update
 *      (estimated) duration at regular frame intervals.
 *      If no (fixed) frames per second rate applies, default conversion will be
 *      based on (estimated) bytes per second (but no default buffer metadata
 *      can be provided in this case).
 *   </para></listitem>
 * </itemizedlist>
 *
 */

/* TODO:
 *  - Better segment handling:
 *    - NEWSEGMENT for gaps
 *    - Not NEWSEGMENT starting at 0 but at first frame timestamp
 *  - GstIndex support
 *  - Seek table generation and subclass seek entry injection
 *  - Accurate seeking
 *  - In push mode provide a queue of adapter-"queued" buffers for upstream
 *    buffer metadata
 *  - Queue buffers/events until caps are set
 *  - Let subclass decide if frames outside the segment should be dropped
 *  - Send queries upstream
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "gstbaseparse.h"

#define MIN_FRAMES_TO_POST_BITRATE 10

GST_DEBUG_CATEGORY_STATIC (gst_base_parse_debug);
#define GST_CAT_DEFAULT gst_base_parse_debug

/* Supported formats */
static GstFormat fmtlist[] = {
  GST_FORMAT_DEFAULT,
  GST_FORMAT_BYTES,
  GST_FORMAT_TIME,
  0
};

#define GST_BASE_PARSE_GET_PRIVATE(obj)  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GST_TYPE_BASE_PARSE, GstBaseParsePrivate))

struct _GstBaseParsePrivate
{
  GstActivateMode pad_mode;

  gint64 duration;
  GstFormat duration_fmt;
  gint64 estimated_duration;

  guint min_frame_size;
  gboolean passthrough;
  guint fps_num, fps_den;
  guint update_interval;
  guint bitrate;
  guint lead_in, lead_out;
  GstClockTime lead_in_ts, lead_out_ts;
  GstBaseParseSeekable seekable;

  gboolean discont;
  gboolean flushing;
  gboolean drain;

  gint64 offset;
  gint64 sync_offset;
  GstClockTime next_ts;
  GstClockTime prev_ts;
  GstClockTime frame_duration;

  guint64 framecount;
  guint64 bytecount;
  guint64 data_bytecount;
  guint64 acc_duration;

  gboolean post_min_bitrate;
  gboolean post_avg_bitrate;
  gboolean post_max_bitrate;
  guint min_bitrate;
  guint avg_bitrate;
  guint max_bitrate;
  guint posted_avg_bitrate;

  GList *pending_events;

  GstBuffer *cache;

  /* index entry storage, either ours or provided */
  GstIndex *index;
  gint index_id;
  gboolean own_index;
  /* seek table entries only maintained if upstream is BYTE seekable */
  gboolean upstream_seekable;
  /* minimum distance between two index entries */
  GstClockTimeDiff idx_interval;
  /* ts and offset of last entry added */
  GstClockTime index_last_ts;
  guint64 index_last_offset;

  /* timestamps currently produced are accurate, e.g. started from 0 onwards */
  gboolean exact_position;
  /* seek events are temporarily kept to match them with newsegments */
  GSList *pending_seeks;
};

typedef struct _GstBaseParseSeek
{
  GstSegment segment;
  gboolean accurate;
  gint64 offset;
  GstClockTime start_ts;
} GstBaseParseSeek;

static GstElementClass *parent_class = NULL;

static void gst_base_parse_class_init (GstBaseParseClass * klass);
static void gst_base_parse_init (GstBaseParse * parse,
    GstBaseParseClass * klass);

GType
gst_base_parse_get_type (void)
{
  static GType base_parse_type = 0;

  if (!base_parse_type) {
    static const GTypeInfo base_parse_info = {
      sizeof (GstBaseParseClass),
      (GBaseInitFunc) NULL,
      (GBaseFinalizeFunc) NULL,
      (GClassInitFunc) gst_base_parse_class_init,
      NULL,
      NULL,
      sizeof (GstBaseParse),
      0,
      (GInstanceInitFunc) gst_base_parse_init,
    };

    base_parse_type = g_type_register_static (GST_TYPE_ELEMENT,
        "GstAudioBaseParseBad", &base_parse_info, G_TYPE_FLAG_ABSTRACT);
  }
  return base_parse_type;
}

static void gst_base_parse_finalize (GObject * object);

static GstStateChangeReturn gst_base_parse_change_state (GstElement * element,
    GstStateChange transition);
static void gst_base_parse_reset (GstBaseParse * parse);

static void gst_base_parse_set_index (GstElement * element, GstIndex * index);
static GstIndex *gst_base_parse_get_index (GstElement * element);

static gboolean gst_base_parse_sink_activate (GstPad * sinkpad);
static gboolean gst_base_parse_sink_activate_push (GstPad * pad,
    gboolean active);
static gboolean gst_base_parse_sink_activate_pull (GstPad * pad,
    gboolean active);
static gboolean gst_base_parse_handle_seek (GstBaseParse * parse,
    GstEvent * event);
static void gst_base_parse_handle_tag (GstBaseParse * parse, GstEvent * event);

static gboolean gst_base_parse_src_event (GstPad * pad, GstEvent * event);
static gboolean gst_base_parse_sink_event (GstPad * pad, GstEvent * event);
static gboolean gst_base_parse_query (GstPad * pad, GstQuery * query);
static gboolean gst_base_parse_sink_setcaps (GstPad * pad, GstCaps * caps);
static const GstQueryType *gst_base_parse_get_querytypes (GstPad * pad);

static GstFlowReturn gst_base_parse_chain (GstPad * pad, GstBuffer * buffer);
static void gst_base_parse_loop (GstPad * pad);

static gboolean gst_base_parse_check_frame (GstBaseParse * parse,
    GstBuffer * buffer, guint * framesize, gint * skipsize);

static GstFlowReturn gst_base_parse_parse_frame (GstBaseParse * parse,
    GstBuffer * buffer);

static gboolean gst_base_parse_sink_eventfunc (GstBaseParse * parse,
    GstEvent * event);

static gboolean gst_base_parse_src_eventfunc (GstBaseParse * parse,
    GstEvent * event);

static void gst_base_parse_drain (GstBaseParse * parse);

static void gst_base_parse_post_bitrates (GstBaseParse * parse,
    gboolean post_min, gboolean post_avg, gboolean post_max);

static void
gst_base_parse_finalize (GObject * object)
{
  GstBaseParse *parse = GST_BASE_PARSE (object);
  GstEvent **p_ev;

  g_mutex_free (parse->parse_lock);
  g_object_unref (parse->adapter);

  if (parse->pending_segment) {
    p_ev = &parse->pending_segment;
    gst_event_replace (p_ev, NULL);
  }
  if (parse->close_segment) {
    p_ev = &parse->close_segment;
    gst_event_replace (p_ev, NULL);
  }

  if (parse->priv->cache) {
    gst_buffer_unref (parse->priv->cache);
    parse->priv->cache = NULL;
  }

  g_list_foreach (parse->priv->pending_events, (GFunc) gst_mini_object_unref,
      NULL);
  g_list_free (parse->priv->pending_events);
  parse->priv->pending_events = NULL;

  if (parse->priv->index) {
    gst_object_unref (parse->priv->index);
    parse->priv->index = NULL;
  }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_base_parse_class_init (GstBaseParseClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = G_OBJECT_CLASS (klass);
  g_type_class_add_private (klass, sizeof (GstBaseParsePrivate));
  parent_class = g_type_class_peek_parent (klass);
  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_base_parse_finalize);

  gstelement_class = (GstElementClass *) klass;
  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_base_parse_change_state);
  gstelement_class->set_index = GST_DEBUG_FUNCPTR (gst_base_parse_set_index);
  gstelement_class->get_index = GST_DEBUG_FUNCPTR (gst_base_parse_get_index);

  /* Default handlers */
  klass->check_valid_frame = gst_base_parse_check_frame;
  klass->parse_frame = gst_base_parse_parse_frame;
  klass->src_event = gst_base_parse_src_eventfunc;
  klass->convert = gst_base_parse_convert_default;

  GST_DEBUG_CATEGORY_INIT (gst_base_parse_debug, "baseparse", 0,
      "baseparse element");
}

static void
gst_base_parse_init (GstBaseParse * parse, GstBaseParseClass * bclass)
{
  GstPadTemplate *pad_template;

  GST_DEBUG_OBJECT (parse, "gst_base_parse_init");

  parse->priv = GST_BASE_PARSE_GET_PRIVATE (parse);

  pad_template =
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (bclass), "sink");
  g_return_if_fail (pad_template != NULL);
  parse->sinkpad = gst_pad_new_from_template (pad_template, "sink");
  gst_pad_set_event_function (parse->sinkpad,
      GST_DEBUG_FUNCPTR (gst_base_parse_sink_event));
  gst_pad_set_setcaps_function (parse->sinkpad,
      GST_DEBUG_FUNCPTR (gst_base_parse_sink_setcaps));
  gst_pad_set_chain_function (parse->sinkpad,
      GST_DEBUG_FUNCPTR (gst_base_parse_chain));
  gst_pad_set_activate_function (parse->sinkpad,
      GST_DEBUG_FUNCPTR (gst_base_parse_sink_activate));
  gst_pad_set_activatepush_function (parse->sinkpad,
      GST_DEBUG_FUNCPTR (gst_base_parse_sink_activate_push));
  gst_pad_set_activatepull_function (parse->sinkpad,
      GST_DEBUG_FUNCPTR (gst_base_parse_sink_activate_pull));
  gst_element_add_pad (GST_ELEMENT (parse), parse->sinkpad);

  GST_DEBUG_OBJECT (parse, "sinkpad created");

  pad_template =
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (bclass), "src");
  g_return_if_fail (pad_template != NULL);
  parse->srcpad = gst_pad_new_from_template (pad_template, "src");
  gst_pad_set_event_function (parse->srcpad,
      GST_DEBUG_FUNCPTR (gst_base_parse_src_event));
  gst_pad_set_query_type_function (parse->srcpad,
      GST_DEBUG_FUNCPTR (gst_base_parse_get_querytypes));
  gst_pad_set_query_function (parse->srcpad,
      GST_DEBUG_FUNCPTR (gst_base_parse_query));
  gst_pad_use_fixed_caps (parse->srcpad);
  gst_element_add_pad (GST_ELEMENT (parse), parse->srcpad);
  GST_DEBUG_OBJECT (parse, "src created");

  parse->parse_lock = g_mutex_new ();
  parse->adapter = gst_adapter_new ();

  parse->priv->pad_mode = GST_ACTIVATE_NONE;

  /* init state */
  gst_base_parse_reset (parse);
  GST_DEBUG_OBJECT (parse, "init ok");
}

static void
gst_base_parse_reset (GstBaseParse * parse)
{
  GST_OBJECT_LOCK (parse);
  gst_segment_init (&parse->segment, GST_FORMAT_TIME);
  parse->priv->duration = -1;
  parse->priv->min_frame_size = 1;
  parse->priv->discont = TRUE;
  parse->priv->flushing = FALSE;
  parse->priv->offset = 0;
  parse->priv->sync_offset = 0;
  parse->priv->update_interval = 50;
  parse->priv->fps_num = parse->priv->fps_den = 0;
  parse->priv->frame_duration = GST_CLOCK_TIME_NONE;
  parse->priv->lead_in = parse->priv->lead_out = 0;
  parse->priv->lead_in_ts = parse->priv->lead_out_ts = 0;
  parse->priv->seekable = GST_BASE_PARSE_SEEK_DEFAULT;
  parse->priv->bitrate = 0;
  parse->priv->framecount = 0;
  parse->priv->bytecount = 0;
  parse->priv->acc_duration = 0;
  parse->priv->estimated_duration = -1;
  parse->priv->next_ts = 0;
  parse->priv->passthrough = FALSE;
  parse->priv->post_min_bitrate = TRUE;
  parse->priv->post_avg_bitrate = TRUE;
  parse->priv->post_max_bitrate = TRUE;
  parse->priv->min_bitrate = G_MAXUINT;
  parse->priv->max_bitrate = 0;
  parse->priv->avg_bitrate = 0;
  parse->priv->posted_avg_bitrate = 0;

  parse->priv->index_last_ts = 0;
  parse->priv->index_last_offset = 0;
  parse->priv->upstream_seekable = FALSE;
  parse->priv->idx_interval = 0;
  parse->priv->exact_position = TRUE;

  if (parse->pending_segment)
    gst_event_unref (parse->pending_segment);

  g_list_foreach (parse->priv->pending_events, (GFunc) gst_mini_object_unref,
      NULL);
  g_list_free (parse->priv->pending_events);
  parse->priv->pending_events = NULL;

  if (parse->priv->cache) {
    gst_buffer_unref (parse->priv->cache);
    parse->priv->cache = NULL;
  }

  g_slist_foreach (parse->priv->pending_seeks, (GFunc) g_free, NULL);
  g_slist_free (parse->priv->pending_seeks);
  parse->priv->pending_seeks = NULL;

  GST_OBJECT_UNLOCK (parse);
}

/**
 * gst_base_parse_check_frame:
 * @parse: #GstBaseParse.
 * @buffer: GstBuffer.
 * @framesize: This will be set to tell the found frame size in bytes.
 * @skipsize: Output parameter that tells how much data needs to be skipped
 *            in order to find the following frame header.
 *
 * Default callback for check_valid_frame.
 * 
 * Returns: Always TRUE.
 */
static gboolean
gst_base_parse_check_frame (GstBaseParse * parse,
    GstBuffer * buffer, guint * framesize, gint * skipsize)
{
  *framesize = GST_BUFFER_SIZE (buffer);
  *skipsize = 0;
  return TRUE;
}


/**
 * gst_base_parse_parse_frame:
 * @parse: #GstBaseParse.
 * @buffer: #GstBuffer.
 *
 * Default callback for parse_frame.
 */
static GstFlowReturn
gst_base_parse_parse_frame (GstBaseParse * parse, GstBuffer * buffer)
{
  if (!GST_BUFFER_TIMESTAMP_IS_VALID (buffer) &&
      GST_CLOCK_TIME_IS_VALID (parse->priv->next_ts)) {
    GST_BUFFER_TIMESTAMP (buffer) = parse->priv->next_ts;
  }
  if (!GST_BUFFER_DURATION_IS_VALID (buffer) &&
      GST_CLOCK_TIME_IS_VALID (parse->priv->frame_duration)) {
    GST_BUFFER_DURATION (buffer) = parse->priv->frame_duration;
  }
  return GST_FLOW_OK;
}

/**
 * gst_base_parse_convert:
 * @parse: #GstBaseParse.
 * @src_format: #GstFormat describing the source format.
 * @src_value: Source value to be converted.
 * @dest_format: #GstFormat defining the converted format.
 * @dest_value: Pointer where the conversion result will be put.
 *
 * Converts using configured "convert" vmethod in #GstBaseParse class.
 *
 * Returns: TRUE if conversion was successful.
 */
static gboolean
gst_base_parse_convert (GstBaseParse * parse,
    GstFormat src_format,
    gint64 src_value, GstFormat dest_format, gint64 * dest_value)
{
  GstBaseParseClass *klass = GST_BASE_PARSE_GET_CLASS (parse);
  gboolean ret;

  g_return_val_if_fail (dest_value != NULL, FALSE);

  if (!klass->convert)
    return FALSE;

  ret = klass->convert (parse, src_format, src_value, dest_format, dest_value);

#ifndef GST_DISABLE_GST_DEBUG
  {
    if (ret) {
      if (src_format == GST_FORMAT_TIME && dest_format == GST_FORMAT_BYTES) {
        GST_LOG_OBJECT (parse,
            "TIME -> BYTES: %" GST_TIME_FORMAT " -> %" G_GINT64_FORMAT,
            GST_TIME_ARGS (src_value), *dest_value);
      } else if (dest_format == GST_FORMAT_TIME &&
          src_format == GST_FORMAT_BYTES) {
        GST_LOG_OBJECT (parse,
            "BYTES -> TIME: %" G_GINT64_FORMAT " -> %" GST_TIME_FORMAT,
            src_value, GST_TIME_ARGS (*dest_value));
      } else {
        GST_LOG_OBJECT (parse,
            "%s -> %s: %" G_GINT64_FORMAT " -> %" G_GINT64_FORMAT,
            GST_STR_NULL (gst_format_get_name (src_format)),
            GST_STR_NULL (gst_format_get_name (dest_format)),
            src_value, *dest_value);
      }
    } else {
      GST_DEBUG_OBJECT (parse, "conversion failed");
    }
  }
#endif

  return ret;
}

/**
 * gst_base_parse_sink_event:
 * @pad: #GstPad that received the event.
 * @event: #GstEvent to be handled.
 *
 * Handler for sink pad events.
 *
 * Returns: TRUE if the event was handled.
 */
static gboolean
gst_base_parse_sink_event (GstPad * pad, GstEvent * event)
{
  GstBaseParse *parse;
  GstBaseParseClass *bclass;
  gboolean handled = FALSE;
  gboolean ret = TRUE;


  parse = GST_BASE_PARSE (gst_pad_get_parent (pad));
  bclass = GST_BASE_PARSE_GET_CLASS (parse);

  GST_DEBUG_OBJECT (parse, "handling event %d", GST_EVENT_TYPE (event));

  /* Cache all events except EOS, NEWSEGMENT and FLUSH_STOP if we have a
   * pending segment */
  if (parse->pending_segment && GST_EVENT_TYPE (event) != GST_EVENT_EOS
      && GST_EVENT_TYPE (event) != GST_EVENT_NEWSEGMENT
      && GST_EVENT_TYPE (event) != GST_EVENT_FLUSH_START
      && GST_EVENT_TYPE (event) != GST_EVENT_FLUSH_STOP) {

    if (GST_EVENT_TYPE (event) == GST_EVENT_TAG)
      /* See if any bitrate tags were posted */
      gst_base_parse_handle_tag (parse, event);

    parse->priv->pending_events =
        g_list_append (parse->priv->pending_events, event);
    ret = TRUE;
  } else {

    if (GST_EVENT_TYPE (event) == GST_EVENT_EOS &&
        parse->priv->framecount < MIN_FRAMES_TO_POST_BITRATE)
      /* We've not posted bitrate tags yet - do so now */
      gst_base_parse_post_bitrates (parse, TRUE, TRUE, TRUE);

    if (bclass->event)
      handled = bclass->event (parse, event);

    if (!handled)
      handled = gst_base_parse_sink_eventfunc (parse, event);

    if (!handled)
      ret = gst_pad_event_default (pad, event);
  }

  gst_object_unref (parse);
  GST_DEBUG_OBJECT (parse, "event handled");
  return ret;
}


/**
 * gst_base_parse_sink_eventfunc:
 * @parse: #GstBaseParse.
 * @event: #GstEvent to be handled.
 *
 * Element-level event handler function.
 *
 * Returns: TRUE if the event was handled and not need forwarding.
 */
static gboolean
gst_base_parse_sink_eventfunc (GstBaseParse * parse, GstEvent * event)
{
  gboolean handled = FALSE;
  GstEvent **eventp;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_NEWSEGMENT:
    {
      gdouble rate, applied_rate;
      GstFormat format;
      gint64 start, stop, pos, next_ts, offset = 0;
      gboolean update;

      gst_event_parse_new_segment_full (event, &update, &rate, &applied_rate,
          &format, &start, &stop, &pos);

      GST_DEBUG_OBJECT (parse, "newseg rate %g, applied rate %g, "
          "format %d, start = %" GST_TIME_FORMAT ", stop = %" GST_TIME_FORMAT
          ", pos = %" GST_TIME_FORMAT, rate, applied_rate, format,
          GST_TIME_ARGS (start), GST_TIME_ARGS (stop), GST_TIME_ARGS (pos));

      if (format == GST_FORMAT_BYTES) {
        GstClockTime seg_start, seg_stop;
        GstBaseParseSeek *seek = NULL;
        GSList *node;

        /* stop time is allowed to be open-ended, but not start & pos */
        seg_stop = GST_CLOCK_TIME_NONE;
        seg_start = 0;
        offset = pos;

        GST_OBJECT_LOCK (parse);
        for (node = parse->priv->pending_seeks; node; node = node->next) {
          GstBaseParseSeek *tmp = node->data;

          if (tmp->offset == pos) {
            seek = tmp;
            break;
          }
        }
        parse->priv->pending_seeks =
            g_slist_remove (parse->priv->pending_seeks, seek);
        GST_OBJECT_UNLOCK (parse);

        if (seek) {
          GST_DEBUG_OBJECT (parse,
              "Matched newsegment to%s seek: %" GST_SEGMENT_FORMAT,
              seek->accurate ? " accurate" : "", &seek->segment);
          seg_start = seek->segment.start;
          seg_stop = seek->segment.stop;
          next_ts = seek->start_ts;
          parse->priv->exact_position = seek->accurate;
          g_free (seek);
        } else {
          /* best attempt convert */
          /* as these are only estimates, stop is kept open-ended to avoid
           * premature cutting */
          gst_base_parse_convert (parse, GST_FORMAT_BYTES, start,
              GST_FORMAT_TIME, (gint64 *) & seg_start);
          parse->priv->exact_position = (start == 0);
          next_ts = seg_start;
        }

        gst_event_unref (event);
        event = gst_event_new_new_segment_full (update, rate, applied_rate,
            GST_FORMAT_TIME, seg_start, seg_stop, seg_start);
        format = GST_FORMAT_TIME;
        start = seg_start;
        stop = seg_stop;
        GST_DEBUG_OBJECT (parse, "Converted incoming segment to TIME. "
            "start = %" GST_TIME_FORMAT ", stop = %" GST_TIME_FORMAT,
            GST_TIME_ARGS (seg_start), GST_TIME_ARGS (seg_stop));
      } else if (format != GST_FORMAT_TIME) {
        /* Unknown incoming segment format. Output a default open-ended 
         * TIME segment */
        gst_event_unref (event);
        event = gst_event_new_new_segment_full (update, rate, applied_rate,
            GST_FORMAT_TIME, 0, GST_CLOCK_TIME_NONE, 0);
        format = GST_FORMAT_TIME;
        next_ts = start = 0;
        stop = GST_CLOCK_TIME_NONE;
      } else {
        /* not considered BYTE seekable if it is talking to us in TIME,
         * whatever else it might claim */
        parse->priv->upstream_seekable = FALSE;
        next_ts = start;
      }

      gst_segment_set_newsegment_full (&parse->segment, update, rate,
          applied_rate, format, start, stop, start);

      /* save the segment for later, right before we push a new buffer so that
       * the caps are fixed and the next linked element can receive
       * the segment. */
      eventp = &parse->pending_segment;
      gst_event_replace (eventp, event);
      gst_event_unref (event);
      handled = TRUE;

      /* but finish the current segment */
      GST_DEBUG_OBJECT (parse, "draining current segment");
      gst_base_parse_drain (parse);
      gst_adapter_clear (parse->adapter);
      parse->priv->offset = offset;
      parse->priv->sync_offset = offset;
      parse->priv->next_ts = next_ts;
      parse->priv->discont = TRUE;
      break;
    }

    case GST_EVENT_FLUSH_START:
      parse->priv->flushing = TRUE;
      handled = gst_pad_push_event (parse->srcpad, event);
      /* Wait for _chain() to exit by taking the srcpad STREAM_LOCK */
      GST_PAD_STREAM_LOCK (parse->srcpad);
      GST_PAD_STREAM_UNLOCK (parse->srcpad);

      break;

    case GST_EVENT_FLUSH_STOP:
      gst_adapter_clear (parse->adapter);
      parse->priv->flushing = FALSE;
      parse->priv->discont = TRUE;
      break;

    case GST_EVENT_EOS:
      gst_base_parse_drain (parse);

      /* If we STILL have zero frames processed, fire an error */
      if (parse->priv->framecount == 0) {
        GST_ELEMENT_ERROR (parse, STREAM, WRONG_TYPE,
            ("No valid frames found before end of stream"), (NULL));
      }
      /* newsegment before eos */
      if (parse->pending_segment) {
        gst_pad_push_event (parse->srcpad, parse->pending_segment);
        parse->pending_segment = NULL;
      }
      break;

    default:
      break;
  }

  return handled;
}


/**
 * gst_base_parse_src_event:
 * @pad: #GstPad that received the event.
 * @event: #GstEvent that was received.
 *
 * Handler for source pad events.
 *
 * Returns: TRUE if the event was handled.
 */
static gboolean
gst_base_parse_src_event (GstPad * pad, GstEvent * event)
{
  GstBaseParse *parse;
  GstBaseParseClass *bclass;
  gboolean handled = FALSE;
  gboolean ret = TRUE;

  parse = GST_BASE_PARSE (gst_pad_get_parent (pad));
  bclass = GST_BASE_PARSE_GET_CLASS (parse);

  GST_DEBUG_OBJECT (parse, "event %d, %s", GST_EVENT_TYPE (event),
      GST_EVENT_TYPE_NAME (event));

  if (bclass->src_event)
    handled = bclass->src_event (parse, event);

  if (!handled)
    ret = gst_pad_event_default (pad, event);
  else
    gst_event_unref (event);

  gst_object_unref (parse);
  return ret;
}


/**
 * gst_base_parse_src_eventfunc:
 * @parse: #GstBaseParse.
 * @event: #GstEvent that was received.
 *
 * Default srcpad event handler.
 *
 * Returns: TRUE if the event was handled and can be dropped.
 */
static gboolean
gst_base_parse_src_eventfunc (GstBaseParse * parse, GstEvent * event)
{
  gboolean handled = FALSE;
  GstBaseParseClass *bclass;

  bclass = GST_BASE_PARSE_GET_CLASS (parse);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
    {
      if (parse->priv->seekable > GST_BASE_PARSE_SEEK_NONE) {
        handled = gst_base_parse_handle_seek (parse, event);
      }
      break;
    }
    default:
      break;
  }
  return handled;
}


/**
 * gst_base_parse_convert_default:
 * @parse: #GstBaseParse.
 * @src_format: #GstFormat describing the source format.
 * @src_value: Source value to be converted.
 * @dest_format: #GstFormat defining the converted format.
 * @dest_value: Pointer where the conversion result will be put.
 *
 * Default implementation of "convert" vmethod in #GstBaseParse class.
 *
 * Returns: TRUE if conversion was successful.
 */
gboolean
gst_base_parse_convert_default (GstBaseParse * parse,
    GstFormat src_format,
    gint64 src_value, GstFormat dest_format, gint64 * dest_value)
{
  gboolean ret = FALSE;
  guint64 bytes, duration;

  if (G_UNLIKELY (src_format == dest_format)) {
    *dest_value = src_value;
    return TRUE;
  }

  if (G_UNLIKELY (src_value == -1)) {
    *dest_value = -1;
    return TRUE;
  }

  if (G_UNLIKELY (src_value == 0)) {
    *dest_value = 0;
    return TRUE;
  }

  /* need at least some frames */
  if (!parse->priv->framecount)
    return FALSE;

  duration = parse->priv->acc_duration / GST_MSECOND;
  bytes = parse->priv->bytecount;

  if (G_UNLIKELY (!duration || !bytes))
    return FALSE;

  if (src_format == GST_FORMAT_BYTES) {
    if (dest_format == GST_FORMAT_TIME) {
      /* BYTES -> TIME conversion */
      GST_DEBUG_OBJECT (parse, "converting bytes -> time");
      *dest_value = gst_util_uint64_scale (src_value, duration, bytes);
      *dest_value *= GST_MSECOND;
      GST_DEBUG_OBJECT (parse, "conversion result: %" G_GINT64_FORMAT " ms",
          *dest_value / GST_MSECOND);
      ret = TRUE;
    }
  } else if (src_format == GST_FORMAT_TIME) {
    if (dest_format == GST_FORMAT_BYTES) {
      GST_DEBUG_OBJECT (parse, "converting time -> bytes");
      *dest_value = gst_util_uint64_scale (src_value / GST_MSECOND, bytes,
          duration);
      GST_DEBUG_OBJECT (parse,
          "time %" G_GINT64_FORMAT " ms in bytes = %" G_GINT64_FORMAT,
          src_value / GST_MSECOND, *dest_value);
      ret = TRUE;
    }
  } else if (src_format == GST_FORMAT_DEFAULT) {
    /* DEFAULT == frame-based */
    if (dest_format == GST_FORMAT_TIME) {
      if (parse->priv->fps_den) {
        *dest_value = gst_util_uint64_scale (src_value,
            GST_SECOND * parse->priv->fps_den, parse->priv->fps_num);
        ret = TRUE;
      }
    } else if (dest_format == GST_FORMAT_BYTES) {
    }
  }

  return ret;
}

/**
 * gst_base_parse_update_duration:
 * @parse: #GstBaseParse.
 *
 */
static void
gst_base_parse_update_duration (GstBaseParse * aacparse)
{
  GstPad *peer;
  GstBaseParse *parse;

  parse = GST_BASE_PARSE (aacparse);

  peer = gst_pad_get_peer (parse->sinkpad);
  if (peer) {
    GstFormat pformat = GST_FORMAT_BYTES;
    gboolean qres = FALSE;
    gint64 ptot, dest_value;

    qres = gst_pad_query_duration (peer, &pformat, &ptot);
    gst_object_unref (GST_OBJECT (peer));
    if (qres) {
      if (gst_base_parse_convert (parse, pformat, ptot,
              GST_FORMAT_TIME, &dest_value))
        parse->priv->estimated_duration = dest_value;
    }
  }
}

static void
gst_base_parse_post_bitrates (GstBaseParse * parse, gboolean post_min,
    gboolean post_avg, gboolean post_max)
{
  GstTagList *taglist = gst_tag_list_new ();

  if (post_min && parse->priv->post_min_bitrate)
    gst_tag_list_add (taglist, GST_TAG_MERGE_REPLACE,
        GST_TAG_MINIMUM_BITRATE, parse->priv->min_bitrate, NULL);

  if (post_avg && parse->priv->post_avg_bitrate) {
    parse->priv->posted_avg_bitrate = parse->priv->avg_bitrate;
    gst_tag_list_add (taglist, GST_TAG_MERGE_REPLACE, GST_TAG_BITRATE,
        parse->priv->avg_bitrate, NULL);
  }

  if (post_max && parse->priv->post_max_bitrate)
    gst_tag_list_add (taglist, GST_TAG_MERGE_REPLACE,
        GST_TAG_MAXIMUM_BITRATE, parse->priv->max_bitrate, NULL);

  GST_DEBUG_OBJECT (parse, "Updated bitrates. Min: %u, Avg: %u, Max: %u",
      parse->priv->min_bitrate, parse->priv->avg_bitrate,
      parse->priv->max_bitrate);

  gst_element_found_tags_for_pad (GST_ELEMENT (parse), parse->srcpad, taglist);
}

/**
 * gst_base_parse_update_bitrates:
 * @parse: #GstBaseParse.
 * @buffer: Current frame as a #GstBuffer
 *
 * Keeps track of the minimum and maximum bitrates, and also maintains a
 * running average bitrate of the stream so far.
 */
static void
gst_base_parse_update_bitrates (GstBaseParse * parse, GstBuffer * buffer)
{
  /* Only update the tag on a 10 kbps delta */
  static const gint update_threshold = 10000;

  GstBaseParseClass *klass;
  guint64 data_len, frame_dur;
  gint overhead = 0, frame_bitrate, old_avg_bitrate;
  gboolean update_min = FALSE, update_avg = FALSE, update_max = FALSE;

  klass = GST_BASE_PARSE_GET_CLASS (parse);

  if (klass->get_frame_overhead) {
    overhead = klass->get_frame_overhead (parse, buffer);
    if (overhead == -1)
      return;
  }

  data_len = GST_BUFFER_SIZE (buffer) - overhead;
  parse->priv->data_bytecount += data_len;

  /* duration should be valid by now,
   * either set by subclass or maybe based on fps settings */
  if (GST_BUFFER_DURATION_IS_VALID (buffer)) {
    /* Calculate duration of a frame from buffer properties */
    frame_dur = GST_BUFFER_DURATION (buffer);
    parse->priv->avg_bitrate = (8 * parse->priv->data_bytecount * GST_SECOND) /
        parse->priv->acc_duration;

  } else {
    /* No way to figure out frame duration (is this even possible?) */
    return;
  }

  /* override if subclass provided bitrate, e.g. metadata based */
  if (parse->priv->bitrate) {
    parse->priv->avg_bitrate = parse->priv->bitrate;
  }

  frame_bitrate = (8 * data_len * GST_SECOND) / frame_dur;

  GST_LOG_OBJECT (parse, "frame bitrate %u, avg bitrate %u", frame_bitrate,
      parse->priv->avg_bitrate);

  if (frame_bitrate < parse->priv->min_bitrate) {
    parse->priv->min_bitrate = frame_bitrate;
    update_min = TRUE;
  }

  if (frame_bitrate > parse->priv->max_bitrate) {
    parse->priv->max_bitrate = frame_bitrate;
    update_max = TRUE;
  }

  old_avg_bitrate = parse->priv->posted_avg_bitrate;
  if ((gint) (old_avg_bitrate - parse->priv->avg_bitrate) > update_threshold ||
      (gint) (parse->priv->avg_bitrate - old_avg_bitrate) > update_threshold)
    update_avg = TRUE;

  /* always post all at threshold time */
  if (parse->priv->framecount == MIN_FRAMES_TO_POST_BITRATE)
    gst_base_parse_post_bitrates (parse, TRUE, TRUE, TRUE);

  if (parse->priv->framecount > MIN_FRAMES_TO_POST_BITRATE &&
      (update_min || update_avg || update_max))
    gst_base_parse_post_bitrates (parse, update_min, update_avg, update_max);

  /* If average bitrate changes that much and no valid (time) duration provided,
   * then post a new duration message so applications can update their cached
   * values */
  if (update_avg && !(parse->priv->duration_fmt == GST_FORMAT_TIME &&
          GST_CLOCK_TIME_IS_VALID (parse->priv->duration)))
    gst_element_post_message (GST_ELEMENT (parse),
        gst_message_new_duration (GST_OBJECT (parse), GST_FORMAT_TIME, -1));
}

/**
 * gst_base_parse_add_index_entry:
 * @parse: #GstBaseParse.
 * @offset: offset of entry
 * @ts: timestamp associated with offset
 * @key: whether entry refers to keyframe
 * @force: add entry disregarding sanity checks
 *
 * Adds an entry to the index associating @offset to @ts.  It is recommended
 * to only add keyframe entries.  @force allows to bypass checks, such as
 * whether the stream is (upstream) seekable, another entry is already "close"
 * to the new entry, etc.
 *
 * Returns: #gboolean indicating whether entry was added
 */
gboolean
gst_base_parse_add_index_entry (GstBaseParse * parse, guint64 offset,
    GstClockTime ts, gboolean key, gboolean force)
{
  gboolean ret = FALSE;
  GstIndexAssociation associations[2];

  GST_LOG_OBJECT (parse, "Adding key=%d index entry %" GST_TIME_FORMAT
      " @ offset 0x%08" G_GINT64_MODIFIER "x", key, GST_TIME_ARGS (ts), offset);

  if (G_LIKELY (!force)) {

    if (!parse->priv->upstream_seekable) {
      GST_DEBUG_OBJECT (parse, "upstream not seekable; discarding");
      goto exit;
    }

    if (parse->priv->index_last_offset >= offset) {
      GST_DEBUG_OBJECT (parse, "already have entries up to offset "
          "0x%08" G_GINT64_MODIFIER "x", parse->priv->index_last_offset);
      goto exit;
    }

    if (GST_CLOCK_DIFF (parse->priv->index_last_ts, ts) <
        parse->priv->idx_interval) {
      GST_DEBUG_OBJECT (parse, "entry too close to last time %" GST_TIME_FORMAT,
          GST_TIME_ARGS (parse->priv->index_last_ts));
      goto exit;
    }
  }

  associations[0].format = GST_FORMAT_TIME;
  associations[0].value = ts;
  associations[1].format = GST_FORMAT_BYTES;
  associations[1].value = offset;

  /* index might change on-the-fly, although that would be nutty app ... */
  GST_OBJECT_LOCK (parse);
  gst_index_add_associationv (parse->priv->index, parse->priv->index_id,
      (key) ? GST_ASSOCIATION_FLAG_KEY_UNIT : GST_ASSOCIATION_FLAG_NONE,
      2, (const GstIndexAssociation *) &associations);
  GST_OBJECT_UNLOCK (parse);

  parse->priv->index_last_offset = offset;
  parse->priv->index_last_ts = ts;

  ret = TRUE;

exit:
  return ret;
}

/* check for seekable upstream, above and beyond a mere query */
static void
gst_base_parse_check_seekability (GstBaseParse * parse)
{
  GstQuery *query;
  gboolean seekable = FALSE;
  gint64 start = -1, stop = -1;
  guint idx_interval = 0;

  query = gst_query_new_seeking (GST_FORMAT_BYTES);
  if (!gst_pad_peer_query (parse->sinkpad, query)) {
    GST_DEBUG_OBJECT (parse, "seeking query failed");
    goto done;
  }

  gst_query_parse_seeking (query, NULL, &seekable, &start, &stop);

  /* try harder to query upstream size if we didn't get it the first time */
  if (seekable && stop == -1) {
    GstFormat fmt = GST_FORMAT_BYTES;

    GST_DEBUG_OBJECT (parse, "doing duration query to fix up unset stop");
    gst_pad_query_peer_duration (parse->sinkpad, &fmt, &stop);
  }

  /* if upstream doesn't know the size, it's likely that it's not seekable in
   * practice even if it technically may be seekable */
  if (seekable && (start != 0 || stop <= start)) {
    GST_DEBUG_OBJECT (parse, "seekable but unknown start/stop -> disable");
    seekable = FALSE;
  }

  /* let's not put every single frame into our index */
  if (seekable) {
    if (stop < 10 * 1024 * 1024)
      idx_interval = 100;
    else if (stop < 100 * 1024 * 1024)
      idx_interval = 500;
    else
      idx_interval = 1000;
  }

done:
  gst_query_unref (query);

  GST_DEBUG_OBJECT (parse, "seekable: %d (%" G_GUINT64_FORMAT " - %"
      G_GUINT64_FORMAT ")", seekable, start, stop);
  parse->priv->upstream_seekable = seekable;

  GST_DEBUG_OBJECT (parse, "idx_interval: %ums", idx_interval);
  parse->priv->idx_interval = idx_interval * GST_MSECOND;
}

/**
 * gst_base_parse_handle_and_push_buffer:
 * @parse: #GstBaseParse.
 * @klass: #GstBaseParseClass.
 * @buffer: #GstBuffer.
 *
 * Parses the frame from given buffer and pushes it forward. Also performs
 * timestamp handling and checks the segment limits.
 *
 * This is called with srcpad STREAM_LOCK held.
 *
 * Returns: #GstFlowReturn
 */
static GstFlowReturn
gst_base_parse_handle_and_push_buffer (GstBaseParse * parse,
    GstBaseParseClass * klass, GstBuffer * buffer)
{
  GstFlowReturn ret;

  if (parse->priv->discont) {
    GST_DEBUG_OBJECT (parse, "marking DISCONT");
    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DISCONT);
    parse->priv->discont = FALSE;
  }

  GST_LOG_OBJECT (parse,
      "parsing frame at offset %" G_GUINT64_FORMAT
      " (%#" G_GINT64_MODIFIER "x) of size %d",
      GST_BUFFER_OFFSET (buffer), GST_BUFFER_OFFSET (buffer),
      GST_BUFFER_SIZE (buffer));

  ret = klass->parse_frame (parse, buffer);

  /* re-use default handler to add missing metadata as-much-as-possible */
  gst_base_parse_parse_frame (parse, buffer);
  if (GST_BUFFER_TIMESTAMP_IS_VALID (buffer) &&
      GST_BUFFER_DURATION_IS_VALID (buffer)) {
    parse->priv->next_ts =
        GST_BUFFER_TIMESTAMP (buffer) + GST_BUFFER_DURATION (buffer);
  } else {
    /* we lost track, do not produce bogus time next time around
     * (probably means parser subclass has given up on parsing as well) */
    GST_DEBUG_OBJECT (parse, "no next fallback timestamp");
    parse->priv->next_ts = GST_CLOCK_TIME_NONE;
  }

  /* First buffers are dropped, this means that the subclass needs more
   * frames to decide on the format and queues them internally */
  /* convert internal flow to OK and mark discont for the next buffer. */
  if (ret == GST_BASE_PARSE_FLOW_DROPPED) {
    gst_buffer_unref (buffer);
    return GST_FLOW_OK;
  } else if (ret != GST_FLOW_OK) {
    return ret;
  }

  return gst_base_parse_push_buffer (parse, buffer);
}

/**
 * gst_base_parse_push_buffer:
 * @parse: #GstBaseParse.
 * @buffer: #GstBuffer.
 *
 * Pushes the buffer downstream, sends any pending events and
 * does some timestamp and segment handling.
 *
 * This must be called with srcpad STREAM_LOCK held.
 *
 * Returns: #GstFlowReturn
 */
GstFlowReturn
gst_base_parse_push_buffer (GstBaseParse * parse, GstBuffer * buffer)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstClockTime last_start = GST_CLOCK_TIME_NONE;
  GstClockTime last_stop = GST_CLOCK_TIME_NONE;
  GstBaseParseClass *klass = GST_BASE_PARSE_GET_CLASS (parse);

  GST_LOG_OBJECT (parse,
      "processing buffer of size %d with ts %" GST_TIME_FORMAT
      ", duration %" GST_TIME_FORMAT, GST_BUFFER_SIZE (buffer),
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)),
      GST_TIME_ARGS (GST_BUFFER_DURATION (buffer)));

  /* some one-time start-up */
  if (G_UNLIKELY (!parse->priv->framecount)) {
    gst_base_parse_check_seekability (parse);
  }

  /* update stats */
  parse->priv->bytecount += GST_BUFFER_SIZE (buffer);
  if (!GST_BUFFER_FLAG_IS_SET (buffer, GST_BASE_PARSE_BUFFER_FLAG_NO_FRAME)) {
    parse->priv->framecount++;
    if (GST_BUFFER_DURATION_IS_VALID (buffer)) {
      parse->priv->acc_duration += GST_BUFFER_DURATION (buffer);
    }
  }
  GST_BUFFER_FLAG_UNSET (buffer, GST_BASE_PARSE_BUFFER_FLAG_NO_FRAME);
  if (parse->priv->update_interval &&
      (parse->priv->framecount % parse->priv->update_interval) == 0)
    gst_base_parse_update_duration (parse);

  gst_base_parse_update_bitrates (parse, buffer);

  if (GST_BUFFER_TIMESTAMP_IS_VALID (buffer))
    last_start = last_stop = GST_BUFFER_TIMESTAMP (buffer);
  if (last_start != GST_CLOCK_TIME_NONE
      && GST_BUFFER_DURATION_IS_VALID (buffer))
    last_stop = last_start + GST_BUFFER_DURATION (buffer);

  /* should have caps by now */
  g_return_val_if_fail (GST_PAD_CAPS (parse->srcpad), GST_FLOW_ERROR);

  gst_buffer_set_caps (buffer, GST_PAD_CAPS (parse->srcpad));

  /* segment adjustment magic; only if we are running the whole show */
  if (!parse->priv->passthrough &&
      (parse->priv->pad_mode == GST_ACTIVATE_PULL ||
          parse->priv->upstream_seekable)) {
    /* segment times are typically estimates,
     * actual frame data might lead subclass to different timestamps,
     * so override segment start from what is supplied there */
    if (G_UNLIKELY (parse->pending_segment && !parse->priv->exact_position &&
            GST_CLOCK_TIME_IS_VALID (last_start))) {
      gst_event_unref (parse->pending_segment);
      parse->segment.start =
          MIN ((guint64) last_start, (guint64) parse->segment.stop);
      GST_DEBUG_OBJECT (parse,
          "adjusting pending segment start to %" GST_TIME_FORMAT,
          GST_TIME_ARGS (parse->segment.start));
      parse->pending_segment =
          gst_event_new_new_segment (FALSE, parse->segment.rate,
          parse->segment.format, parse->segment.start, parse->segment.stop,
          parse->segment.start);
    }
    /* handle gaps, e.g. non-zero start-time, in as much not handled by above */
    if (GST_CLOCK_TIME_IS_VALID (parse->segment.last_stop) &&
        GST_CLOCK_TIME_IS_VALID (last_start)) {
      GstClockTimeDiff diff;

      /* only send newsegments with increasing start times,
       * otherwise if these go back and forth downstream (sinks) increase
       * accumulated time and running_time */
      diff = GST_CLOCK_DIFF (parse->segment.last_stop, last_start);
      if (G_UNLIKELY (diff > 2 * GST_SECOND && last_start > parse->segment.start
              && (!GST_CLOCK_TIME_IS_VALID (parse->segment.stop) ||
                  last_start < parse->segment.stop))) {
        GST_DEBUG_OBJECT (parse,
            "Gap of %" G_GINT64_FORMAT " ns detected in stream "
            "(%" GST_TIME_FORMAT " -> %" GST_TIME_FORMAT "). "
            "Sending updated NEWSEGMENT events", diff,
            GST_TIME_ARGS (parse->segment.last_stop),
            GST_TIME_ARGS (last_start));
        if (G_UNLIKELY (parse->pending_segment)) {
          gst_event_unref (parse->pending_segment);
          parse->segment.start = last_start;
          parse->pending_segment =
              gst_event_new_new_segment (FALSE, parse->segment.rate,
              parse->segment.format, parse->segment.start, parse->segment.stop,
              parse->segment.start);
        } else {
          /* send newsegment events such that the gap is not accounted in
           * accum time, hence running_time */
          /* close ahead of gap */
          gst_pad_push_event (parse->srcpad,
              gst_event_new_new_segment (TRUE, parse->segment.rate,
                  parse->segment.format, parse->segment.last_stop,
                  parse->segment.last_stop, parse->segment.last_stop));
          /* skip gap */
          gst_pad_push_event (parse->srcpad,
              gst_event_new_new_segment (FALSE, parse->segment.rate,
                  parse->segment.format, last_start, parse->segment.stop,
                  last_start));
        }
        /* align segment view with downstream,
         * prevents double-counting accum when closing segment */
        gst_segment_set_newsegment (&parse->segment, FALSE,
            parse->segment.rate, parse->segment.format, last_start,
            parse->segment.stop, last_start);
        parse->segment.last_stop = last_start;
      }
    }
  }

  /* and should then also be linked downstream, so safe to send some events */
  if (parse->priv->pad_mode == GST_ACTIVATE_PULL) {
    if (G_UNLIKELY (parse->close_segment)) {
      GST_DEBUG_OBJECT (parse, "loop sending close segment");
      gst_pad_push_event (parse->srcpad, parse->close_segment);
      parse->close_segment = NULL;
    }

    if (G_UNLIKELY (parse->pending_segment)) {
      GST_DEBUG_OBJECT (parse, "loop push pending segment");
      gst_pad_push_event (parse->srcpad, parse->pending_segment);
      parse->pending_segment = NULL;
    }
  } else {
    if (G_UNLIKELY (parse->pending_segment)) {
      GST_DEBUG_OBJECT (parse, "chain pushing a pending segment");
      gst_pad_push_event (parse->srcpad, parse->pending_segment);
      parse->pending_segment = NULL;
    }
  }

  if (G_UNLIKELY (parse->priv->pending_events)) {
    GList *l;

    for (l = parse->priv->pending_events; l != NULL; l = l->next) {
      gst_pad_push_event (parse->srcpad, GST_EVENT (l->data));
    }
    g_list_free (parse->priv->pending_events);
    parse->priv->pending_events = NULL;
  }

  if (parse->priv->upstream_seekable && parse->priv->exact_position &&
      GST_BUFFER_TIMESTAMP_IS_VALID (buffer))
    gst_base_parse_add_index_entry (parse, GST_BUFFER_OFFSET (buffer),
        GST_BUFFER_TIMESTAMP (buffer),
        !GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT), FALSE);

  if (klass->pre_push_buffer)
    ret = klass->pre_push_buffer (parse, buffer);
  else
    ret = GST_BASE_PARSE_FLOW_CLIP;

  if (ret == GST_BASE_PARSE_FLOW_CLIP) {
    if (GST_BUFFER_TIMESTAMP_IS_VALID (buffer) &&
        GST_CLOCK_TIME_IS_VALID (parse->segment.stop) &&
        GST_BUFFER_TIMESTAMP (buffer) >
        parse->segment.stop + parse->priv->lead_out_ts) {
      GST_LOG_OBJECT (parse, "Dropped frame, after segment");
      ret = GST_FLOW_UNEXPECTED;
    } else if (GST_BUFFER_TIMESTAMP_IS_VALID (buffer) &&
        GST_BUFFER_DURATION_IS_VALID (buffer) &&
        GST_CLOCK_TIME_IS_VALID (parse->segment.start) &&
        GST_BUFFER_TIMESTAMP (buffer) + GST_BUFFER_DURATION (buffer) +
        parse->priv->lead_in_ts < parse->segment.start) {
      GST_LOG_OBJECT (parse, "Dropped frame, before segment");
      ret = GST_BASE_PARSE_FLOW_DROPPED;
    } else {
      ret = GST_FLOW_OK;
    }
  }

  if (ret == GST_BASE_PARSE_FLOW_DROPPED) {
    GST_LOG_OBJECT (parse, "frame (%d bytes) dropped",
        GST_BUFFER_SIZE (buffer));
    gst_buffer_unref (buffer);
    ret = GST_FLOW_OK;
  } else if (ret == GST_FLOW_OK) {
    ret = gst_pad_push (parse->srcpad, buffer);
    GST_LOG_OBJECT (parse, "frame (%d bytes) pushed: %s",
        GST_BUFFER_SIZE (buffer), gst_flow_get_name (ret));
  } else {
    gst_buffer_unref (buffer);
    GST_LOG_OBJECT (parse, "frame (%d bytes) not pushed: %s",
        GST_BUFFER_SIZE (buffer), gst_flow_get_name (ret));
    /* if we are not sufficiently in control, let upstream decide on EOS */
    if (ret == GST_FLOW_UNEXPECTED &&
        (parse->priv->passthrough ||
            (parse->priv->pad_mode == GST_ACTIVATE_PUSH &&
                !parse->priv->upstream_seekable)))
      ret = GST_FLOW_OK;
  }

  /* Update current running segment position */
  if (ret == GST_FLOW_OK && last_stop != GST_CLOCK_TIME_NONE &&
      parse->segment.last_stop < last_stop)
    gst_segment_set_last_stop (&parse->segment, GST_FORMAT_TIME, last_stop);

  return ret;
}


/**
 * gst_base_parse_drain:
 * @parse: #GstBaseParse.
 *
 * Drains the adapter until it is empty. It decreases the min_frame_size to
 * match the current adapter size and calls chain method until the adapter
 * is emptied or chain returns with error.
 */
static void
gst_base_parse_drain (GstBaseParse * parse)
{
  guint avail;

  GST_DEBUG_OBJECT (parse, "draining");
  parse->priv->drain = TRUE;

  for (;;) {
    avail = gst_adapter_available (parse->adapter);
    if (!avail)
      break;

    if (gst_base_parse_chain (parse->sinkpad, NULL) != GST_FLOW_OK) {
      break;
    }

    /* nothing changed, maybe due to truncated frame; break infinite loop */
    if (avail == gst_adapter_available (parse->adapter)) {
      GST_DEBUG_OBJECT (parse, "no change during draining; flushing");
      gst_adapter_clear (parse->adapter);
    }
  }

  parse->priv->drain = FALSE;
}

/* small helper that checks whether we have been trying to resync too long */
static inline GstFlowReturn
gst_base_parse_check_sync (GstBaseParse * parse)
{
  if (G_UNLIKELY (parse->priv->discont &&
          parse->priv->offset - parse->priv->sync_offset > 2 * 1024 * 1024)) {
    GST_ELEMENT_ERROR (parse, STREAM, DECODE,
        ("Failed to parse stream"), (NULL));
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}


/**
 * gst_base_parse_chain:
 * @pad: #GstPad.
 * @buffer: #GstBuffer.
 *
 * Returns: #GstFlowReturn.
 */
static GstFlowReturn
gst_base_parse_chain (GstPad * pad, GstBuffer * buffer)
{
  GstBaseParseClass *bclass;
  GstBaseParse *parse;
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *outbuf = NULL;
  GstBuffer *tmpbuf = NULL;
  guint fsize = 0;
  gint skip = -1;
  const guint8 *data;
  guint min_size;
  GstClockTime timestamp;

  parse = GST_BASE_PARSE (GST_OBJECT_PARENT (pad));
  bclass = GST_BASE_PARSE_GET_CLASS (parse);

  if (G_LIKELY (buffer)) {
    GST_LOG_OBJECT (parse, "buffer size: %d, offset = %" G_GINT64_FORMAT,
        GST_BUFFER_SIZE (buffer), GST_BUFFER_OFFSET (buffer));
    if (G_UNLIKELY (parse->priv->passthrough)) {
      buffer = gst_buffer_make_metadata_writable (buffer);
      return gst_base_parse_push_buffer (parse, buffer);
    } else
      gst_adapter_push (parse->adapter, buffer);
  }

  /* Parse and push as many frames as possible */
  /* Stop either when adapter is empty or we are flushing */
  while (!parse->priv->flushing) {
    tmpbuf = gst_buffer_new ();

    /* Synchronization loop */
    for (;;) {
      GST_BASE_PARSE_LOCK (parse);
      min_size = parse->priv->min_frame_size;
      GST_BASE_PARSE_UNLOCK (parse);

      if (G_UNLIKELY (parse->priv->drain)) {
        min_size = gst_adapter_available (parse->adapter);
        GST_DEBUG_OBJECT (parse, "draining, data left: %d", min_size);
        if (G_UNLIKELY (!min_size)) {
          gst_buffer_unref (tmpbuf);
          goto done;
        }
      }

      /* Collect at least min_frame_size bytes */
      if (gst_adapter_available (parse->adapter) < min_size) {
        GST_DEBUG_OBJECT (parse, "not enough data available (only %d bytes)",
            gst_adapter_available (parse->adapter));
        gst_buffer_unref (tmpbuf);
        goto done;
      }

      data = gst_adapter_peek (parse->adapter, min_size);
      GST_BUFFER_DATA (tmpbuf) = (guint8 *) data;
      GST_BUFFER_SIZE (tmpbuf) = min_size;
      GST_BUFFER_OFFSET (tmpbuf) = parse->priv->offset;
      GST_BUFFER_FLAG_SET (tmpbuf, GST_MINI_OBJECT_FLAG_READONLY);

      if (parse->priv->discont) {
        GST_DEBUG_OBJECT (parse, "marking DISCONT");
        GST_BUFFER_FLAG_SET (tmpbuf, GST_BUFFER_FLAG_DISCONT);
      }

      skip = -1;
      if (bclass->check_valid_frame (parse, tmpbuf, &fsize, &skip)) {
        if (gst_adapter_available (parse->adapter) < fsize) {
          GST_DEBUG_OBJECT (parse,
              "found valid frame but not enough data available (only %d bytes)",
              gst_adapter_available (parse->adapter));
          gst_buffer_unref (tmpbuf);
          goto done;
        }
        break;
      }
      if (skip > 0) {
        GST_LOG_OBJECT (parse, "finding sync, skipping %d bytes", skip);
        gst_adapter_flush (parse->adapter, skip);
        parse->priv->offset += skip;
        if (!parse->priv->discont)
          parse->priv->sync_offset = parse->priv->offset;
        parse->priv->discont = TRUE;
      } else if (skip == -1) {
        /* subclass didn't touch this value. By default we skip 1 byte */
        GST_LOG_OBJECT (parse, "finding sync, skipping 1 byte");
        gst_adapter_flush (parse->adapter, 1);
        parse->priv->offset++;
        if (!parse->priv->discont)
          parse->priv->sync_offset = parse->priv->offset;
        parse->priv->discont = TRUE;
      }
      /* There is a possibility that subclass set the skip value to zero.
         This means that it has probably found a frame but wants to ask
         more data (by increasing the min_size) to be sure of this. */
      if ((ret = gst_base_parse_check_sync (parse)) != GST_FLOW_OK) {
        gst_buffer_unref (tmpbuf);
        goto done;
      }
    }
    gst_buffer_unref (tmpbuf);
    tmpbuf = NULL;

    if (skip > 0) {
      /* Subclass found the sync, but still wants to skip some data */
      GST_LOG_OBJECT (parse, "skipping %d bytes", skip);
      gst_adapter_flush (parse->adapter, skip);
      parse->priv->offset += skip;
    }

    /* Grab lock to prevent a race with FLUSH_START handler */
    GST_PAD_STREAM_LOCK (parse->srcpad);

    /* FLUSH_START event causes the "flushing" flag to be set. In this
     * case we can leave the frame pushing loop */
    if (parse->priv->flushing) {
      GST_PAD_STREAM_UNLOCK (parse->srcpad);
      break;
    }

    /* FIXME: Would it be more efficient to make a subbuffer instead? */
    outbuf = gst_adapter_take_buffer (parse->adapter, fsize);
    outbuf = gst_buffer_make_metadata_writable (outbuf);

    /* Subclass may want to know the data offset */
    GST_BUFFER_OFFSET (outbuf) = parse->priv->offset;
    parse->priv->offset += fsize;

    /* move along with upstream timestamp (if any),
     * but interpolate in between */
    timestamp = gst_adapter_prev_timestamp (parse->adapter, NULL);
    if (GST_CLOCK_TIME_IS_VALID (timestamp) &&
        (parse->priv->prev_ts != timestamp)) {
      parse->priv->prev_ts = parse->priv->next_ts = timestamp;
    }

    ret = gst_base_parse_handle_and_push_buffer (parse, bclass, outbuf);
    GST_PAD_STREAM_UNLOCK (parse->srcpad);

    if (ret != GST_FLOW_OK) {
      GST_LOG_OBJECT (parse, "push returned %d", ret);
      break;
    }
  }

done:
  GST_LOG_OBJECT (parse, "chain leaving");
  return ret;
}

/* pull @size bytes at current offset,
 * i.e. at least try to and possibly return a shorter buffer if near the end */
static GstFlowReturn
gst_base_parse_pull_range (GstBaseParse * parse, guint size,
    GstBuffer ** buffer)
{
  GstFlowReturn ret = GST_FLOW_OK;

  g_return_val_if_fail (buffer != NULL, GST_FLOW_ERROR);

  /* Caching here actually makes much less difference than one would expect.
   * We do it mainly to avoid pulling buffers of 1 byte all the time */
  if (parse->priv->cache) {
    gint64 cache_offset = GST_BUFFER_OFFSET (parse->priv->cache);
    gint cache_size = GST_BUFFER_SIZE (parse->priv->cache);

    if (cache_offset <= parse->priv->offset &&
        (parse->priv->offset + size) <= (cache_offset + cache_size)) {
      *buffer = gst_buffer_create_sub (parse->priv->cache,
          parse->priv->offset - cache_offset, size);
      GST_BUFFER_OFFSET (*buffer) = parse->priv->offset;
      return GST_FLOW_OK;
    }
    /* not enough data in the cache, free cache and get a new one */
    gst_buffer_unref (parse->priv->cache);
    parse->priv->cache = NULL;
  }

  /* refill the cache */
  ret =
      gst_pad_pull_range (parse->sinkpad, parse->priv->offset, MAX (size,
          64 * 1024), &parse->priv->cache);
  if (ret != GST_FLOW_OK) {
    parse->priv->cache = NULL;
    return ret;
  }

  if (GST_BUFFER_SIZE (parse->priv->cache) >= size) {
    *buffer = gst_buffer_create_sub (parse->priv->cache, 0, size);
    GST_BUFFER_OFFSET (*buffer) = parse->priv->offset;
    return GST_FLOW_OK;
  }

  /* Not possible to get enough data, try a last time with
   * requesting exactly the size we need */
  gst_buffer_unref (parse->priv->cache);
  parse->priv->cache = NULL;

  ret = gst_pad_pull_range (parse->sinkpad, parse->priv->offset, size,
      &parse->priv->cache);

  if (ret != GST_FLOW_OK) {
    GST_DEBUG_OBJECT (parse, "pull_range returned %d", ret);
    *buffer = NULL;
    return ret;
  }

  if (GST_BUFFER_SIZE (parse->priv->cache) < size) {
    GST_DEBUG_OBJECT (parse, "Returning short buffer at offset %"
        G_GUINT64_FORMAT ": wanted %u bytes, got %u bytes", parse->priv->offset,
        size, GST_BUFFER_SIZE (parse->priv->cache));

    *buffer = parse->priv->cache;
    parse->priv->cache = NULL;

    return GST_FLOW_OK;
  }

  *buffer = gst_buffer_create_sub (parse->priv->cache, 0, size);
  GST_BUFFER_OFFSET (*buffer) = parse->priv->offset;

  return GST_FLOW_OK;
}

/**
 * gst_base_parse_loop:
 * @pad: GstPad
 *
 * Loop that is used in pull mode to retrieve data from upstream.
 */
static void
gst_base_parse_loop (GstPad * pad)
{
  GstBaseParse *parse;
  GstBaseParseClass *klass;
  GstBuffer *buffer, *outbuf;
  gboolean ret = FALSE;
  guint fsize = 0, min_size;
  gint skip = 0;

  parse = GST_BASE_PARSE (gst_pad_get_parent (pad));
  klass = GST_BASE_PARSE_GET_CLASS (parse);

  /* TODO: Check if we reach segment stop limit */

  while (TRUE) {

    GST_BASE_PARSE_LOCK (parse);
    min_size = parse->priv->min_frame_size;
    GST_BASE_PARSE_UNLOCK (parse);

    ret = gst_base_parse_pull_range (parse, min_size, &buffer);

    if (ret == GST_FLOW_UNEXPECTED)
      goto eos;
    else if (ret != GST_FLOW_OK)
      goto pause;

    if (parse->priv->discont) {
      GST_DEBUG_OBJECT (parse, "marking DISCONT");
      GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DISCONT);
    }

    /* if we got a short read, inform subclass we are draining leftover
     * and no more is to be expected */
    if (GST_BUFFER_SIZE (buffer) < min_size)
      parse->priv->drain = TRUE;

    skip = -1;
    if (klass->check_valid_frame (parse, buffer, &fsize, &skip)) {
      parse->priv->drain = FALSE;
      break;
    }
    parse->priv->drain = FALSE;
    if (skip > 0) {
      GST_LOG_OBJECT (parse, "finding sync, skipping %d bytes", skip);
      parse->priv->offset += skip;
      if (!parse->priv->discont)
        parse->priv->sync_offset = parse->priv->offset;
      parse->priv->discont = TRUE;
    } else if (skip == -1) {
      GST_LOG_OBJECT (parse, "finding sync, skipping 1 byte");
      parse->priv->offset++;
      if (!parse->priv->discont)
        parse->priv->sync_offset = parse->priv->offset;
      parse->priv->discont = TRUE;
    }
    /* skip == 0 should imply subclass set min_size to need more data ... */
    GST_DEBUG_OBJECT (parse, "finding sync...");
    gst_buffer_unref (buffer);
    if ((ret = gst_base_parse_check_sync (parse)) != GST_FLOW_OK) {
      goto done;
    }
  }

  if (fsize <= GST_BUFFER_SIZE (buffer)) {
    outbuf = gst_buffer_create_sub (buffer, 0, fsize);
    GST_BUFFER_OFFSET (outbuf) = GST_BUFFER_OFFSET (buffer);
    gst_buffer_unref (buffer);
  } else {
    gst_buffer_unref (buffer);
    ret = gst_base_parse_pull_range (parse, fsize, &outbuf);

    if (ret == GST_FLOW_UNEXPECTED)
      goto eos;
    else if (ret != GST_FLOW_OK)
      goto pause;
    if (GST_BUFFER_SIZE (outbuf) < fsize)
      goto eos;
  }

  parse->priv->offset += fsize;

  /* Does the subclass want to skip too? */
  if (skip > 0)
    parse->priv->offset += skip;

  /* This always unrefs the outbuf, even if error occurs */
  ret = gst_base_parse_handle_and_push_buffer (parse, klass, outbuf);

  if (ret != GST_FLOW_OK)
    goto pause;

done:
  gst_object_unref (parse);
  return;

  /* ERRORS */
eos:
  {
    ret = GST_FLOW_UNEXPECTED;
    GST_DEBUG_OBJECT (parse, "eos");
    /* fall-through */
  }
pause:
  {
    gboolean push_eos = FALSE;

    GST_DEBUG_OBJECT (parse, "pausing task, reason %s",
        gst_flow_get_name (ret));
    gst_pad_pause_task (parse->sinkpad);

    if (ret == GST_FLOW_UNEXPECTED) {
      /* handle end-of-stream/segment */
      if (parse->segment.flags & GST_SEEK_FLAG_SEGMENT) {
        gint64 stop;

        if ((stop = parse->segment.stop) == -1)
          stop = parse->segment.duration;

        GST_DEBUG_OBJECT (parse, "sending segment_done");

        gst_element_post_message
            (GST_ELEMENT_CAST (parse),
            gst_message_new_segment_done (GST_OBJECT_CAST (parse),
                GST_FORMAT_TIME, stop));
      } else {
        /* If we STILL have zero frames processed, fire an error */
        if (parse->priv->framecount == 0) {
          GST_ELEMENT_ERROR (parse, STREAM, WRONG_TYPE,
              ("No valid frames found before end of stream"), (NULL));
        }
        push_eos = TRUE;
      }
    } else if (ret == GST_FLOW_NOT_LINKED || ret < GST_FLOW_UNEXPECTED) {
      /* for fatal errors we post an error message, wrong-state is
       * not fatal because it happens due to flushes and only means
       * that we should stop now. */
      GST_ELEMENT_ERROR (parse, STREAM, FAILED, (NULL),
          ("streaming stopped, reason %s", gst_flow_get_name (ret)));
      push_eos = TRUE;
    }
    if (push_eos) {
      /* newsegment before eos */
      if (parse->pending_segment) {
        gst_pad_push_event (parse->srcpad, parse->pending_segment);
        parse->pending_segment = NULL;
      }
      gst_pad_push_event (parse->srcpad, gst_event_new_eos ());
    }
    gst_object_unref (parse);
  }
}


/**
 * gst_base_parse_sink_activate:
 * @sinkpad: #GstPad to be activated.
 *
 * Returns: TRUE if activation succeeded.
 */
static gboolean
gst_base_parse_sink_activate (GstPad * sinkpad)
{
  GstBaseParse *parse;
  gboolean result = TRUE;

  parse = GST_BASE_PARSE (gst_pad_get_parent (sinkpad));

  GST_DEBUG_OBJECT (parse, "sink activate");

  if (gst_pad_check_pull_range (sinkpad)) {
    GST_DEBUG_OBJECT (parse, "trying to activate in pull mode");
    result = gst_pad_activate_pull (sinkpad, TRUE);
  } else {
    GST_DEBUG_OBJECT (parse, "trying to activate in push mode");
    result = gst_pad_activate_push (sinkpad, TRUE);
  }

  GST_DEBUG_OBJECT (parse, "sink activate return %d", result);
  gst_object_unref (parse);
  return result;
}


/**
 * gst_base_parse_activate:
 * @parse: #GstBaseParse.
 * @active: TRUE if element will be activated, FALSE if disactivated.
 *
 * Returns: TRUE if the operation succeeded.
 */
static gboolean
gst_base_parse_activate (GstBaseParse * parse, gboolean active)
{
  GstBaseParseClass *klass;
  gboolean result = FALSE;

  GST_DEBUG_OBJECT (parse, "activate");

  klass = GST_BASE_PARSE_GET_CLASS (parse);

  if (active) {
    if (parse->priv->pad_mode == GST_ACTIVATE_NONE && klass->start)
      result = klass->start (parse);
  } else {
    /* We must make sure streaming has finished before resetting things
     * and calling the ::stop vfunc */
    GST_PAD_STREAM_LOCK (parse->sinkpad);
    GST_PAD_STREAM_UNLOCK (parse->sinkpad);

    if (parse->priv->pad_mode != GST_ACTIVATE_NONE && klass->stop)
      result = klass->stop (parse);

    parse->priv->pad_mode = GST_ACTIVATE_NONE;
  }
  GST_DEBUG_OBJECT (parse, "activate: %d", result);
  return result;
}


/**
 * gst_base_parse_sink_activate_push:
 * @pad: #GstPad to be (de)activated.
 * @active: TRUE when activating, FALSE when deactivating.
 *
 * Returns: TRUE if (de)activation succeeded.
 */
static gboolean
gst_base_parse_sink_activate_push (GstPad * pad, gboolean active)
{
  gboolean result = TRUE;
  GstBaseParse *parse;

  parse = GST_BASE_PARSE (gst_pad_get_parent (pad));

  GST_DEBUG_OBJECT (parse, "sink activate push");

  result = gst_base_parse_activate (parse, active);

  if (result)
    parse->priv->pad_mode = active ? GST_ACTIVATE_PUSH : GST_ACTIVATE_NONE;

  GST_DEBUG_OBJECT (parse, "sink activate push: %d", result);

  gst_object_unref (parse);
  return result;
}


/**
 * gst_base_parse_sink_activate_pull:
 * @sinkpad: #GstPad to be (de)activated.
 * @active: TRUE when activating, FALSE when deactivating.
 *
 * Returns: TRUE if (de)activation succeeded.
 */
static gboolean
gst_base_parse_sink_activate_pull (GstPad * sinkpad, gboolean active)
{
  gboolean result = FALSE;
  GstBaseParse *parse;

  parse = GST_BASE_PARSE (gst_pad_get_parent (sinkpad));

  GST_DEBUG_OBJECT (parse, "activate pull");

  result = gst_base_parse_activate (parse, active);

  if (result) {
    if (active) {
      parse->pending_segment = gst_event_new_new_segment (FALSE,
          parse->segment.rate, parse->segment.format,
          parse->segment.start, parse->segment.stop, parse->segment.last_stop);
      result &= gst_pad_start_task (sinkpad,
          (GstTaskFunction) gst_base_parse_loop, sinkpad);
    } else {
      result &= gst_pad_stop_task (sinkpad);
    }
  }

  if (result)
    parse->priv->pad_mode = active ? GST_ACTIVATE_PULL : GST_ACTIVATE_NONE;

  GST_DEBUG_OBJECT (parse, "sink activate pull: %d", result);

  gst_object_unref (parse);
  return result;
}


/**
 * gst_base_parse_set_duration:
 * @parse: #GstBaseParse.
 * @fmt: #GstFormat.
 * @duration: duration value.
 *
 * Sets the duration of the currently playing media. Subclass can use this
 * when it is able to determine duration and/or notices a change in the media
 * duration.  Alternatively, if @interval is non-zero (default), then stream
 * duration is determined based on estimated bitrate, and updated every @interval
 * frames. */
void
gst_base_parse_set_duration (GstBaseParse * parse,
    GstFormat fmt, gint64 duration, gint interval)
{
  g_return_if_fail (parse != NULL);

  GST_BASE_PARSE_LOCK (parse);
  if (duration != parse->priv->duration) {
    GstMessage *m;

    m = gst_message_new_duration (GST_OBJECT (parse), fmt, duration);
    gst_element_post_message (GST_ELEMENT (parse), m);

    /* TODO: what about duration tag? */
  }
  parse->priv->duration = duration;
  parse->priv->duration_fmt = fmt;
  GST_DEBUG_OBJECT (parse, "set duration: %" G_GINT64_FORMAT, duration);
  if (fmt == GST_FORMAT_TIME && GST_CLOCK_TIME_IS_VALID (duration)) {
    if (interval != 0) {
      GST_DEBUG_OBJECT (parse, "valid duration provided, disabling estimate");
      interval = 0;
    }
  }
  GST_DEBUG_OBJECT (parse, "set update interval: %d", interval);
  parse->priv->update_interval = interval;
  GST_BASE_PARSE_UNLOCK (parse);
}

/**
 * gst_base_parse_set_seek:
 * @parse: #GstBaseParse.
 * @seek: #GstBaseParseSeekable.
 * @abitrate: average bitrate.
 *
 * Sets whether and how the media is seekable (in time).
 * Also optionally provides average bitrate detected in media (if non-zero),
 * e.g. based on metadata, as it will be posted to the application.
 *
 * By default, announced average bitrate is estimated, and seekability is assumed
 * possible based on estimated bitrate.
 */
void
gst_base_parse_set_seek (GstBaseParse * parse,
    GstBaseParseSeekable seek, guint bitrate)
{
  parse->priv->seekable = seek;
  parse->priv->bitrate = bitrate;
}


/**
 * gst_base_parse_set_min_frame_size:
 * @parse: #GstBaseParse.
 * @min_size: Minimum size of the data that this base class should give to
 *            subclass.
 *
 * Subclass can use this function to tell the base class that it needs to
 * give at least #min_size buffers.
 */
void
gst_base_parse_set_min_frame_size (GstBaseParse * parse, guint min_size)
{
  g_return_if_fail (parse != NULL);

  GST_BASE_PARSE_LOCK (parse);
  parse->priv->min_frame_size = min_size;
  GST_LOG_OBJECT (parse, "set frame_min_size: %d", min_size);
  GST_BASE_PARSE_UNLOCK (parse);
}

/**
 * gst_base_transform_set_passthrough:
 * @trans: the #GstBaseParse to set
 * @passthrough: boolean indicating passthrough mode.
 *
 * Set passthrough mode for this parser.  If operating in passthrough,
 * incoming buffers are pushed through unmodified.
 */
void
gst_base_parse_set_passthrough (GstBaseParse * parse, gboolean passthrough)
{
  g_return_if_fail (parse != NULL);

  GST_BASE_PARSE_LOCK (parse);
  parse->priv->passthrough = passthrough;
  GST_LOG_OBJECT (parse, "set passthrough: %d", passthrough);
  GST_BASE_PARSE_UNLOCK (parse);
}

/**
 * gst_base_transform_set_frame_props:
 * @parse: the #GstBaseParse to set
 * @fps_num: frames per second (numerator).
 * @fps_den: frames per second (denominator).
 * @lead_in: frames needed before a segment for subsequent decode
 * @lead_out: frames needed after a segment
 *
 * If frames per second is configured, parser can take care of buffer duration
 * and timestamping.  When performing segment clipping, or seeking to a specific
 * location, a corresponding decoder might need an initial @lead_in and a
 * following @lead_out number of frames to ensure the desired segment is
 * entirely filled upon decoding.
 */
void
gst_base_parse_set_frame_props (GstBaseParse * parse, guint fps_num,
    guint fps_den, guint lead_in, guint lead_out)
{
  g_return_if_fail (parse != NULL);

  GST_BASE_PARSE_LOCK (parse);
  parse->priv->fps_num = fps_num;
  parse->priv->fps_den = fps_den;
  if (!fps_num || !fps_den) {
    GST_DEBUG_OBJECT (parse, "invalid fps (%d/%d), ignoring parameters",
        fps_num, fps_den);
    fps_num = fps_den = 0;
    parse->priv->frame_duration = GST_CLOCK_TIME_NONE;
    parse->priv->lead_in = parse->priv->lead_out = 0;
    parse->priv->lead_in_ts = parse->priv->lead_out_ts = 0;
  } else {
    parse->priv->frame_duration =
        gst_util_uint64_scale (GST_SECOND, fps_den, fps_num);
    parse->priv->lead_in = lead_in;
    parse->priv->lead_out = lead_out;
    parse->priv->lead_in_ts =
        gst_util_uint64_scale (GST_SECOND, fps_den * lead_in, fps_num);
    parse->priv->lead_out_ts =
        gst_util_uint64_scale (GST_SECOND, fps_den * lead_out, fps_num);
  }
  GST_LOG_OBJECT (parse, "set fps: %d/%d => duration: %" G_GINT64_FORMAT " ms",
      fps_num, fps_den, parse->priv->frame_duration / GST_MSECOND);
  GST_LOG_OBJECT (parse, "set lead in: %d frames = %" G_GUINT64_FORMAT " ms, "
      "lead out: %d frames = %" G_GUINT64_FORMAT " ms",
      lead_in, parse->priv->lead_in_ts / GST_MSECOND,
      lead_out, parse->priv->lead_out_ts / GST_MSECOND);
  GST_BASE_PARSE_UNLOCK (parse);
}

/**
 * gst_base_transform_get_sync:
 * @parse: the #GstBaseParse to query
 *
 * Returns: TRUE if parser is considered 'in sync'.  That is, frames have been
 * continuously successfully parsed and pushed.
 */
gboolean
gst_base_parse_get_sync (GstBaseParse * parse)
{
  gboolean ret;

  g_return_val_if_fail (parse != NULL, FALSE);

  GST_BASE_PARSE_LOCK (parse);
  /* losing sync is pretty much a discont (and vice versa), no ? */
  ret = !parse->priv->discont;
  GST_BASE_PARSE_UNLOCK (parse);

  GST_DEBUG_OBJECT (parse, "sync: %d", ret);
  return ret;
}

/**
 * gst_base_transform_get_drain:
 * @parse: the #GstBaseParse to query
 *
 * Returns: TRUE if parser is currently 'draining'.  That is, leftover data
 * (e.g. in FLUSH or EOS situation) is being parsed.
 */
gboolean
gst_base_parse_get_drain (GstBaseParse * parse)
{
  gboolean ret;

  g_return_val_if_fail (parse != NULL, FALSE);

  GST_BASE_PARSE_LOCK (parse);
  /* losing sync is pretty much a discont (and vice versa), no ? */
  ret = parse->priv->drain;
  GST_BASE_PARSE_UNLOCK (parse);

  GST_DEBUG_OBJECT (parse, "drain: %d", ret);
  return ret;
}

static gboolean
gst_base_parse_get_duration (GstBaseParse * parse, GstFormat format,
    GstClockTime * duration)
{
  gboolean res = FALSE;

  g_return_val_if_fail (duration != NULL, FALSE);

  *duration = GST_CLOCK_TIME_NONE;
  if (parse->priv->duration != -1 && format == parse->priv->duration_fmt) {
    GST_LOG_OBJECT (parse, "using provided duration");
    *duration = parse->priv->duration;
    res = TRUE;
  } else if (parse->priv->duration != -1) {
    GST_LOG_OBJECT (parse, "converting provided duration");
    res = gst_base_parse_convert (parse, parse->priv->duration_fmt,
        parse->priv->duration, format, (gint64 *) duration);
  } else if (format == GST_FORMAT_TIME && parse->priv->estimated_duration != -1) {
    GST_LOG_OBJECT (parse, "using estimated duration");
    *duration = parse->priv->estimated_duration;
    res = TRUE;
  }

  GST_LOG_OBJECT (parse, "res: %d, duration %" GST_TIME_FORMAT, res,
      GST_TIME_ARGS (*duration));
  return res;
}

/**
 * gst_base_parse_get_querytypes:
 * @pad: GstPad
 *
 * Returns: A table of #GstQueryType items describing supported query types.
 */
static const GstQueryType *
gst_base_parse_get_querytypes (GstPad * pad)
{
  static const GstQueryType list[] = {
    GST_QUERY_POSITION,
    GST_QUERY_DURATION,
    GST_QUERY_FORMATS,
    GST_QUERY_SEEKING,
    GST_QUERY_CONVERT,
    0
  };

  return list;
}


/**
 * gst_base_parse_query:
 * @pad: #GstPad.
 * @query: #GstQuery.
 *
 * Returns: TRUE on success.
 */
static gboolean
gst_base_parse_query (GstPad * pad, GstQuery * query)
{
  GstBaseParse *parse;
  GstBaseParseClass *klass;
  gboolean res = FALSE;

  parse = GST_BASE_PARSE (GST_PAD_PARENT (pad));
  klass = GST_BASE_PARSE_GET_CLASS (parse);

  GST_LOG_OBJECT (parse, "handling query: %" GST_PTR_FORMAT, query);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_POSITION:
    {
      gint64 dest_value;
      GstFormat format;

      GST_DEBUG_OBJECT (parse, "position query");
      gst_query_parse_position (query, &format, NULL);

      g_mutex_lock (parse->parse_lock);
      if (format == GST_FORMAT_BYTES) {
        dest_value = parse->priv->offset;
        res = TRUE;
      } else if (format == parse->segment.format &&
          GST_CLOCK_TIME_IS_VALID (parse->segment.last_stop)) {
        dest_value = parse->segment.last_stop;
        res = TRUE;
      }
      g_mutex_unlock (parse->parse_lock);

      if (res)
        gst_query_set_position (query, format, dest_value);
      else {
        res = gst_pad_query_default (pad, query);
        if (!res) {
          /* no precise result, upstream no idea either, then best estimate */
          /* priv->offset is updated in both PUSH/PULL modes */
          g_mutex_lock (parse->parse_lock);
          res = gst_base_parse_convert (parse,
              GST_FORMAT_BYTES, parse->priv->offset, format, &dest_value);
          g_mutex_unlock (parse->parse_lock);
        }
      }
      break;
    }
    case GST_QUERY_DURATION:
    {
      GstFormat format;
      GstClockTime duration;

      GST_DEBUG_OBJECT (parse, "duration query");
      gst_query_parse_duration (query, &format, NULL);

      /* consult upstream */
      res = gst_pad_query_default (pad, query);

      /* otherwise best estimate from us */
      if (!res) {
        g_mutex_lock (parse->parse_lock);
        res = gst_base_parse_get_duration (parse, format, &duration);
        g_mutex_unlock (parse->parse_lock);
        if (res)
          gst_query_set_duration (query, format, duration);
      }
      break;
    }
    case GST_QUERY_SEEKING:
    {
      GstFormat fmt;
      GstClockTime duration = GST_CLOCK_TIME_NONE;
      gboolean seekable = FALSE;

      GST_DEBUG_OBJECT (parse, "seeking query");
      gst_query_parse_seeking (query, &fmt, NULL, NULL, NULL);

      /* consult upstream */
      res = gst_pad_query_default (pad, query);

      /* we may be able to help if in TIME */
      if (fmt == GST_FORMAT_TIME &&
          parse->priv->seekable > GST_BASE_PARSE_SEEK_NONE) {
        gst_query_parse_seeking (query, &fmt, &seekable, NULL, NULL);
        /* already OK if upstream takes care */
        GST_LOG_OBJECT (parse, "upstream handled %d, seekable %d",
            res, seekable);
        if (!(res && seekable)) {
          /* TODO maybe also check upstream provides proper duration ? */
          seekable = TRUE;
          if (!gst_base_parse_get_duration (parse, GST_FORMAT_TIME, &duration)
              || duration == -1) {
            seekable = FALSE;
          } else {
            GstQuery *q;

            q = gst_query_new_seeking (GST_FORMAT_BYTES);
            if (!gst_pad_peer_query (parse->sinkpad, q)) {
              seekable = FALSE;
            } else {
              gst_query_parse_seeking (q, &fmt, &seekable, NULL, NULL);
            }
            GST_LOG_OBJECT (parse, "upstream BYTE handled %d, seekable %d",
                res, seekable);
            gst_query_unref (q);
          }
          gst_query_set_seeking (query, GST_FORMAT_TIME, seekable, 0, duration);
          res = TRUE;
        }
      }
      break;
    }
    case GST_QUERY_FORMATS:
      gst_query_set_formatsv (query, 3, fmtlist);
      res = TRUE;
      break;
    case GST_QUERY_CONVERT:
    {
      GstFormat src_format, dest_format;
      gint64 src_value, dest_value;

      gst_query_parse_convert (query, &src_format, &src_value,
          &dest_format, &dest_value);

      res = gst_base_parse_convert (parse, src_format, src_value,
          dest_format, &dest_value);
      if (res) {
        gst_query_set_convert (query, src_format, src_value,
            dest_format, dest_value);
      }
      break;
    }
    default:
      res = gst_pad_query_default (pad, query);
      break;
  }
  return res;
}

static gint64
gst_base_parse_find_offset (GstBaseParse * parse, GstClockTime time,
    gboolean before, GstClockTime * _ts)
{
  gint64 bytes = 0, ts = 0;
  GstIndexEntry *entry = NULL;

  if (time == GST_CLOCK_TIME_NONE) {
    ts = time;
    bytes = -1;
    goto exit;
  }

  GST_OBJECT_LOCK (parse);
  if (parse->priv->index) {
    /* Let's check if we have an index entry for that time */
    entry = gst_index_get_assoc_entry (parse->priv->index,
        parse->priv->index_id,
        before ? GST_INDEX_LOOKUP_BEFORE : GST_INDEX_LOOKUP_AFTER,
        GST_ASSOCIATION_FLAG_KEY_UNIT, GST_FORMAT_TIME, time);
  }

  if (entry) {
    gst_index_entry_assoc_map (entry, GST_FORMAT_BYTES, &bytes);
    gst_index_entry_assoc_map (entry, GST_FORMAT_TIME, &ts);

    GST_DEBUG_OBJECT (parse, "found index entry for %" GST_TIME_FORMAT
        " at %" GST_TIME_FORMAT ", offset %" G_GINT64_FORMAT,
        GST_TIME_ARGS (time), GST_TIME_ARGS (ts), bytes);
  } else {
    GST_DEBUG_OBJECT (parse, "no index entry found for %" GST_TIME_FORMAT,
        GST_TIME_ARGS (time));
    if (!before) {
      bytes = -1;
      ts = GST_CLOCK_TIME_NONE;
    }
  }
  GST_OBJECT_UNLOCK (parse);

exit:
  if (_ts)
    *_ts = ts;

  return bytes;
}


/**
 * gst_base_parse_handle_seek:
 * @parse: #GstBaseParse.
 * @event: #GstEvent.
 *
 * Returns: TRUE if seek succeeded.
 */
static gboolean
gst_base_parse_handle_seek (GstBaseParse * parse, GstEvent * event)
{
  GstBaseParseClass *klass;
  gdouble rate;
  GstFormat format;
  GstSeekFlags flags;
  GstSeekType cur_type = GST_SEEK_TYPE_NONE, stop_type;
  gboolean flush, update, res = TRUE, accurate;
  gint64 cur, stop, seekpos, seekstop;
  GstSegment seeksegment = { 0, };
  GstFormat dstformat;
  GstClockTime start_ts;

  klass = GST_BASE_PARSE_GET_CLASS (parse);

  gst_event_parse_seek (event, &rate, &format, &flags,
      &cur_type, &cur, &stop_type, &stop);

  GST_DEBUG_OBJECT (parse, "seek to format %s, "
      "start type %d at %" GST_TIME_FORMAT ", end type %d at %"
      GST_TIME_FORMAT, gst_format_get_name (format),
      cur_type, GST_TIME_ARGS (cur), stop_type, GST_TIME_ARGS (stop));

  /* no negative rates yet */
  if (rate < 0.0)
    goto negative_rate;

  if (cur_type != GST_SEEK_TYPE_SET ||
      (stop_type != GST_SEEK_TYPE_SET && stop_type != GST_SEEK_TYPE_NONE))
    goto wrong_type;

  /* For any format other than TIME, see if upstream handles
   * it directly or fail. For TIME, try upstream, but do it ourselves if
   * it fails upstream */
  if (format != GST_FORMAT_TIME) {
    /* default action delegates to upstream */
    return FALSE;
  } else {
    gst_event_ref (event);
    if (gst_pad_push_event (parse->sinkpad, event)) {
      return TRUE;
    }
  }

  /* get flush flag */
  flush = flags & GST_SEEK_FLAG_FLUSH;

  /* copy segment, we need this because we still need the old
   * segment when we close the current segment. */
  memcpy (&seeksegment, &parse->segment, sizeof (GstSegment));

  GST_DEBUG_OBJECT (parse, "configuring seek");
  gst_segment_set_seek (&seeksegment, rate, format, flags,
      cur_type, cur, stop_type, stop, &update);

  /* accurate seeking implies seek tables are used to obtain position,
   * and the requested segment is maintained exactly, not adjusted any way */
  accurate = flags & GST_SEEK_FLAG_ACCURATE;

  /* maybe we can be accurate for (almost) free */
  if (seeksegment.last_stop <= parse->priv->index_last_ts + 20 * GST_SECOND) {
    GST_DEBUG_OBJECT (parse,
        "seek position %" GST_TIME_FORMAT " <= %" GST_TIME_FORMAT
        "accurate seek possible", GST_TIME_ARGS (seeksegment.last_stop),
        GST_TIME_ARGS (parse->priv->index_last_ts));
    accurate = TRUE;
  }
  if (accurate) {
    GstClockTime startpos = seeksegment.last_stop;

    /* accurate requested, so ... seek a bit before target */
    if (startpos < parse->priv->lead_in_ts)
      startpos = 0;
    else
      startpos -= parse->priv->lead_in_ts;
    seekpos = gst_base_parse_find_offset (parse, startpos, TRUE, &start_ts);
    seekstop = gst_base_parse_find_offset (parse, seeksegment.stop, FALSE,
        NULL);
  } else {
    start_ts = seeksegment.last_stop;
    dstformat = GST_FORMAT_BYTES;
    if (!gst_pad_query_convert (parse->srcpad, format, seeksegment.last_stop,
            &dstformat, &seekpos))
      goto convert_failed;
    if (!gst_pad_query_convert (parse->srcpad, format, seeksegment.stop,
            &dstformat, &seekstop))
      goto convert_failed;
  }

  GST_DEBUG_OBJECT (parse,
      "seek position %" G_GINT64_FORMAT " in bytes: %" G_GINT64_FORMAT, cur,
      seekpos);
  GST_DEBUG_OBJECT (parse,
      "seek stop %" G_GINT64_FORMAT " in bytes: %" G_GINT64_FORMAT,
      seeksegment.stop, seekstop);

  if (parse->priv->pad_mode == GST_ACTIVATE_PULL) {
    gint64 last_stop;

    GST_DEBUG_OBJECT (parse, "seek in PULL mode");

    if (flush) {
      if (parse->srcpad) {
        GST_DEBUG_OBJECT (parse, "sending flush start");
        gst_pad_push_event (parse->srcpad, gst_event_new_flush_start ());
      }
    } else {
      gst_pad_pause_task (parse->sinkpad);
    }

    /* we should now be able to grab the streaming thread because we stopped it
     * with the above flush/pause code */
    GST_PAD_STREAM_LOCK (parse->sinkpad);

    /* save current position */
    last_stop = parse->segment.last_stop;
    GST_DEBUG_OBJECT (parse, "stopped streaming at %" G_GINT64_FORMAT,
        last_stop);

    /* now commit to new position */

    /* prepare for streaming again */
    if (flush) {
      GST_DEBUG_OBJECT (parse, "sending flush stop");
      gst_pad_push_event (parse->srcpad, gst_event_new_flush_stop ());
    } else {
      if (parse->close_segment)
        gst_event_unref (parse->close_segment);

      parse->close_segment = gst_event_new_new_segment (TRUE,
          parse->segment.rate, parse->segment.format,
          parse->segment.accum, parse->segment.last_stop, parse->segment.accum);

      /* keep track of our last_stop */
      seeksegment.accum = parse->segment.last_stop;

      GST_DEBUG_OBJECT (parse, "Created close seg format %d, "
          "start = %" GST_TIME_FORMAT ", stop = %" GST_TIME_FORMAT
          ", pos = %" GST_TIME_FORMAT, format,
          GST_TIME_ARGS (parse->segment.accum),
          GST_TIME_ARGS (parse->segment.last_stop),
          GST_TIME_ARGS (parse->segment.accum));
    }

    memcpy (&parse->segment, &seeksegment, sizeof (GstSegment));

    /* store the newsegment event so it can be sent from the streaming thread. */
    if (parse->pending_segment)
      gst_event_unref (parse->pending_segment);

    /* This will be sent later in _loop() */
    parse->pending_segment =
        gst_event_new_new_segment (FALSE, parse->segment.rate,
        parse->segment.format, parse->segment.last_stop, parse->segment.stop,
        parse->segment.last_stop);

    GST_DEBUG_OBJECT (parse, "Created newseg format %d, "
        "start = %" GST_TIME_FORMAT ", stop = %" GST_TIME_FORMAT
        ", pos = %" GST_TIME_FORMAT, format,
        GST_TIME_ARGS (parse->segment.last_stop),
        GST_TIME_ARGS (stop), GST_TIME_ARGS (parse->segment.last_stop));

    /* mark discont if we are going to stream from another position. */
    if (seekpos != parse->priv->offset) {
      GST_DEBUG_OBJECT (parse,
          "mark DISCONT, we did a seek to another position");
      parse->priv->offset = seekpos;
      parse->priv->discont = TRUE;
      parse->priv->next_ts = start_ts;
      parse->priv->sync_offset = seekpos;
      parse->priv->exact_position = accurate;
    }

    /* Start streaming thread if paused */
    gst_pad_start_task (parse->sinkpad,
        (GstTaskFunction) gst_base_parse_loop, parse->sinkpad);

    GST_PAD_STREAM_UNLOCK (parse->sinkpad);
  } else {
    GstEvent *new_event;
    GstBaseParseSeek *seek;

    /* The only thing we need to do in PUSH-mode is to send the
       seek event (in bytes) to upstream. Segment / flush handling happens
       in corresponding src event handlers */
    GST_DEBUG_OBJECT (parse, "seek in PUSH mode");
    new_event = gst_event_new_seek (rate, GST_FORMAT_BYTES, flush,
        GST_SEEK_TYPE_SET, seekpos, stop_type, seekstop);

    /* store segment info so its precise details can be reconstructed when
     * receiving newsegment;
     * this matters for all details when accurate seeking,
     * is most useful to preserve NONE stop time otherwise */
    seek = g_new0 (GstBaseParseSeek, 1);
    seek->segment = seeksegment;
    seek->accurate = accurate;
    seek->offset = seekpos;
    seek->start_ts = start_ts;
    GST_OBJECT_LOCK (parse);
    /* less optimal, but preserves order */
    parse->priv->pending_seeks =
        g_slist_append (parse->priv->pending_seeks, seek);
    GST_OBJECT_UNLOCK (parse);

    res = gst_pad_push_event (parse->sinkpad, new_event);

    if (!res) {
      GST_OBJECT_LOCK (parse);
      parse->priv->pending_seeks =
          g_slist_remove (parse->priv->pending_seeks, seek);
      GST_OBJECT_UNLOCK (parse);
      g_free (seek);
    }
  }

done:
  return res;

  /* ERRORS */
negative_rate:
  {
    GST_DEBUG_OBJECT (parse, "negative playback rates are not supported yet.");
    res = FALSE;
    goto done;
  }
wrong_type:
  {
    GST_DEBUG_OBJECT (parse, "unsupported seek type.");
    res = FALSE;
    goto done;
  }
convert_failed:
  {
    GST_DEBUG_OBJECT (parse, "conversion TIME to BYTES failed.");
    res = FALSE;
    goto done;
  }
}

/**
 * gst_base_parse_handle_tag:
 * @parse: #GstBaseParse.
 * @event: #GstEvent.
 *
 * Checks if bitrates are available from upstream tags so that we don't
 * override them later
 */
static void
gst_base_parse_handle_tag (GstBaseParse * parse, GstEvent * event)
{
  GstTagList *taglist = NULL;
  guint tmp;

  gst_event_parse_tag (event, &taglist);

  if (gst_tag_list_get_uint (taglist, GST_TAG_MINIMUM_BITRATE, &tmp))
    parse->priv->post_min_bitrate = FALSE;
  if (gst_tag_list_get_uint (taglist, GST_TAG_BITRATE, &tmp))
    parse->priv->post_avg_bitrate = FALSE;
  if (gst_tag_list_get_uint (taglist, GST_TAG_MAXIMUM_BITRATE, &tmp))
    parse->priv->post_max_bitrate = FALSE;
}

/**
 * gst_base_parse_sink_setcaps:
 * @pad: #GstPad.
 * @caps: #GstCaps.
 *
 * Returns: TRUE if caps were accepted.
 */
static gboolean
gst_base_parse_sink_setcaps (GstPad * pad, GstCaps * caps)
{
  GstBaseParse *parse;
  GstBaseParseClass *klass;
  gboolean res = TRUE;

  parse = GST_BASE_PARSE (GST_PAD_PARENT (pad));
  klass = GST_BASE_PARSE_GET_CLASS (parse);

  GST_DEBUG_OBJECT (parse, "caps: %" GST_PTR_FORMAT, caps);

  if (klass->set_sink_caps)
    res = klass->set_sink_caps (parse, caps);

  return res && gst_pad_set_caps (pad, caps);
}

static void
gst_base_parse_set_index (GstElement * element, GstIndex * index)
{
  GstBaseParse *parse = GST_BASE_PARSE (element);

  GST_OBJECT_LOCK (parse);
  if (parse->priv->index)
    gst_object_unref (parse->priv->index);
  if (index) {
    parse->priv->index = gst_object_ref (index);
    gst_index_get_writer_id (index, GST_OBJECT (element),
        &parse->priv->index_id);
    parse->priv->own_index = FALSE;
  } else
    parse->priv->index = NULL;
  GST_OBJECT_UNLOCK (parse);
}

static GstIndex *
gst_base_parse_get_index (GstElement * element)
{
  GstBaseParse *parse = GST_BASE_PARSE (element);
  GstIndex *result = NULL;

  GST_OBJECT_LOCK (parse);
  if (parse->priv->index)
    result = gst_object_ref (parse->priv->index);
  GST_OBJECT_UNLOCK (parse);

  return result;
}

static GstStateChangeReturn
gst_base_parse_change_state (GstElement * element, GstStateChange transition)
{
  GstBaseParse *parse;
  GstStateChangeReturn result;

  parse = GST_BASE_PARSE (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      /* If this is our own index destroy it as the
       * old entries might be wrong for the new stream */
      if (parse->priv->own_index) {
        gst_object_unref (parse->priv->index);
        parse->priv->index = NULL;
        parse->priv->own_index = FALSE;
      }

      /* If no index was created, generate one */
      if (G_UNLIKELY (!parse->priv->index)) {
        GST_DEBUG_OBJECT (parse, "no index provided creating our own");

        parse->priv->index = gst_index_factory_make ("memindex");
        gst_index_get_writer_id (parse->priv->index, GST_OBJECT (parse),
            &parse->priv->index_id);
        parse->priv->own_index = TRUE;
      }
      break;
    default:
      break;
  }

  result = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_base_parse_reset (parse);
      break;
    default:
      break;
  }

  return result;
}