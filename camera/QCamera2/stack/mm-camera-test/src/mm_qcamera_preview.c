/*
Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "mm_qcamera_dbg.h"
#include "mm_qcamera_app.h"
#include <assert.h>
#include <sys/mman.h>
#include <semaphore.h>

static void mm_app_metadata_notify_cb(mm_camera_super_buf_t *bufs,
                                     void *user_data)
{
  int i = 0;
  mm_camera_channel_t *channel = NULL;
  mm_camera_stream_t *p_stream = NULL;
  mm_camera_test_obj_t *pme = (mm_camera_test_obj_t *)user_data;
  mm_camera_buf_def_t *frame = bufs->bufs[0];
  cam_metadata_info_t *metadata;

  /* find channel */
  for (i = 0; i < MM_CHANNEL_TYPE_MAX; i++) {
      if (pme->channels[i].ch_id == bufs->ch_id) {
          channel = &pme->channels[i];
          break;
      }
  }
  /* find preview stream */
  for (i = 0; i < channel->num_streams; i++) {
      if (channel->streams[i].s_config.stream_info->stream_type == CAM_STREAM_TYPE_METADATA) {
          p_stream = &channel->streams[i];
          break;
      }
  }
  /* find preview frame */
  for (i = 0; i < bufs->num_bufs; i++) {
      if (bufs->bufs[i]->stream_id == p_stream->s_id) {
          frame = bufs->bufs[i];
          break;
      }
  }

  if (NULL == p_stream) {
      CDBG_ERROR("%s: cannot find metadata stream", __func__);
      return;
  }
  metadata = frame->buffer;

  if (MM_CAMERA_OK != pme->cam->ops->qbuf(bufs->camera_handle,
                                          bufs->ch_id,
                                          frame)) {
      CDBG_ERROR("%s: Failed in Preview Qbuf\n", __func__);
  }
  mm_app_cache_ops((mm_camera_app_meminfo_t *)frame->mem_info,
                   ION_IOC_INV_CACHES);
}

static void mm_app_preview_notify_cb(mm_camera_super_buf_t *bufs,
                                     void *user_data)
{
    int i = 0;
    mm_camera_channel_t *channel = NULL;
    mm_camera_stream_t *p_stream = NULL;
    mm_camera_buf_def_t *frame = bufs->bufs[0];
    mm_camera_test_obj_t *pme = (mm_camera_test_obj_t *)user_data;

    CDBG("%s: BEGIN - length=%d, frame idx = %d\n",
         __func__, frame->frame_len, frame->frame_idx);

    /* find channel */
    for (i = 0; i < MM_CHANNEL_TYPE_MAX; i++) {
        if (pme->channels[i].ch_id == bufs->ch_id) {
            channel = &pme->channels[i];
            break;
        }
    }
    /* find preview stream */
    for (i = 0; i < channel->num_streams; i++) {
        if (channel->streams[i].s_config.stream_info->stream_type == CAM_STREAM_TYPE_PREVIEW) {
            p_stream = &channel->streams[i];
            break;
        }
    }
    /* find preview frame */
    for (i = 0; i < bufs->num_bufs; i++) {
        if (bufs->bufs[i]->stream_id == p_stream->s_id) {
            frame = bufs->bufs[i];
            break;
        }
    }

    if (NULL == p_stream) {
        CDBG_ERROR("%s: cannot find preview stream", __func__);
        return;
    }

#ifdef DUMP_PRV_IN_FILE
    {
      char file_name[64];
      snprintf(file_name, sizeof(file_name), "P_C%d", pme->cam->camera_handle);
      mm_app_dump_frame(frame, file_name, "yuv", frame->frame_idx);
    }
#endif

    if (MM_CAMERA_OK != pme->cam->ops->qbuf(bufs->camera_handle,
                                            bufs->ch_id,
                                            frame)) {
        CDBG_ERROR("%s: Failed in Preview Qbuf\n", __func__);
    }
    mm_app_cache_ops((mm_camera_app_meminfo_t *)frame->mem_info,
                     ION_IOC_INV_CACHES);

    CDBG("%s: END\n", __func__);
}

static void mm_app_zsl_notify_cb(mm_camera_super_buf_t *bufs,
                                 void *user_data)
{
    int rc = 0;
    int i = 0;
    mm_camera_test_obj_t *pme = (mm_camera_test_obj_t *)user_data;
    mm_camera_channel_t *channel = NULL;
    mm_camera_stream_t *p_stream = NULL;
    mm_camera_stream_t *m_stream = NULL;
    mm_camera_stream_t *md_stream = NULL;
    mm_camera_buf_def_t *p_frame = NULL;
    mm_camera_buf_def_t *m_frame = NULL;
    mm_camera_buf_def_t *md_frame = NULL;

    CDBG("%s: BEGIN\n", __func__);

    /* find channel */
    for (i = 0; i < MM_CHANNEL_TYPE_MAX; i++) {
        if (pme->channels[i].ch_id == bufs->ch_id) {
            channel = &pme->channels[i];
            break;
        }
    }
    if (NULL == channel) {
        CDBG_ERROR("%s: Wrong channel id (%d)", __func__, bufs->ch_id);
        return;
    }

    /* find preview stream */
    for (i = 0; i < channel->num_streams; i++) {
        if (channel->streams[i].s_config.stream_info->stream_type == CAM_STREAM_TYPE_PREVIEW) {
            p_stream = &channel->streams[i];
            break;
        }
    }
    if (NULL == p_stream) {
        CDBG_ERROR("%s: cannot find preview stream", __func__);
        return;
    }

    /* find snapshot stream */
    for (i = 0; i < channel->num_streams; i++) {
        if (channel->streams[i].s_config.stream_info->stream_type == CAM_STREAM_TYPE_SNAPSHOT) {
            m_stream = &channel->streams[i];
            break;
        }
    }
    if (NULL == m_stream) {
        CDBG_ERROR("%s: cannot find snapshot stream", __func__);
        return;
    }

    /* find metadata stream */
    for (i = 0; i < channel->num_streams; i++) {
        if (channel->streams[i].s_config.stream_info->stream_type == CAM_STREAM_TYPE_METADATA) {
            md_stream = &channel->streams[i];
            break;
        }
    }
    if (NULL == md_stream) {
        CDBG_ERROR("%s: cannot find metadata stream", __func__);
    }

    /* find preview frame */
    for (i = 0; i < bufs->num_bufs; i++) {
        if (bufs->bufs[i]->stream_id == p_stream->s_id) {
            p_frame = bufs->bufs[i];
            break;
        }
    }

    if(md_stream) {
      /* find metadata frame */
      for (i = 0; i < bufs->num_bufs; i++) {
          if (bufs->bufs[i]->stream_id == md_stream->s_id) {
              md_frame = bufs->bufs[i];
              break;
          }
      }
    }
    /* find snapshot frame */
    for (i = 0; i < bufs->num_bufs; i++) {
        if (bufs->bufs[i]->stream_id == m_stream->s_id) {
            m_frame = bufs->bufs[i];
            break;
        }
    }

    if (!m_frame || !p_frame) {
        CDBG_ERROR("%s: cannot find preview/snapshot frame", __func__);
        return;
    }

    CDBG("%s: ZSL CB with fb_fd = %d, m_frame = 0x%x, p_frame = 0x%x \n", __func__, pme->fb_fd, (uint32_t )m_frame, (uint32_t )p_frame);

    if ( 0 < pme->fb_fd ) {
        mm_app_overlay_display(pme, p_frame->fd);
    } else {
        mm_app_dump_frame(p_frame, "zsl_preview", "yuv", p_frame->frame_idx);
        mm_app_dump_frame(m_frame, "zsl_main", "yuv", m_frame->frame_idx);
    }

    if ( pme->encodeJpeg ) {
        pme->jpeg_buf.buf.buffer = (uint8_t *)malloc(m_frame->frame_len);
        if ( NULL == pme->jpeg_buf.buf.buffer ) {
            CDBG_ERROR("%s: error allocating jpeg output buffer", __func__);
            goto exit;
        }

        pme->jpeg_buf.buf.frame_len = m_frame->frame_len;
        /* create a new jpeg encoding session */
        rc = createEncodingSession(pme, m_stream, m_frame);
        if (0 != rc) {
            CDBG_ERROR("%s: error creating jpeg session", __func__);
            free(pme->jpeg_buf.buf.buffer);
            goto exit;
        }

        /* start jpeg encoding job */
        rc = encodeData(pme, bufs, m_stream);
        pme->encodeJpeg = 0;
    } else {
        if (MM_CAMERA_OK != pme->cam->ops->qbuf(bufs->camera_handle,
                                                bufs->ch_id,
                                                m_frame)) {
            CDBG_ERROR("%s: Failed in main Qbuf\n", __func__);
        }
        mm_app_cache_ops((mm_camera_app_meminfo_t *)m_frame->mem_info,
                         ION_IOC_INV_CACHES);
    }

exit:

    if (MM_CAMERA_OK != pme->cam->ops->qbuf(bufs->camera_handle,
                                            bufs->ch_id,
                                            p_frame)) {
        CDBG_ERROR("%s: Failed in preview Qbuf\n", __func__);
    }
    mm_app_cache_ops((mm_camera_app_meminfo_t *)p_frame->mem_info,
                     ION_IOC_INV_CACHES);

    if(md_frame) {
      if (MM_CAMERA_OK != pme->cam->ops->qbuf(bufs->camera_handle,
                                              bufs->ch_id,
                                              md_frame)) {
          CDBG_ERROR("%s: Failed in metadata Qbuf\n", __func__);
      }
      mm_app_cache_ops((mm_camera_app_meminfo_t *)md_frame->mem_info,
                       ION_IOC_INV_CACHES);
    }

    CDBG("%s: END\n", __func__);
}

mm_camera_stream_t * mm_app_add_metadata_stream(mm_camera_test_obj_t *test_obj,
                                               mm_camera_channel_t *channel,
                                               mm_camera_buf_notify_t stream_cb,
                                               void *userdata,
                                               uint8_t num_bufs)
{
    int rc = MM_CAMERA_OK;
    mm_camera_stream_t *stream = NULL;
    cam_capability_t *cam_cap = (cam_capability_t *)(test_obj->cap_buf.buf.buffer);

    stream = mm_app_add_stream(test_obj, channel);
    if (NULL == stream) {
        CDBG_ERROR("%s: add stream failed\n", __func__);
        return NULL;
    }

    stream->s_config.mem_vtbl.get_bufs = mm_app_stream_initbuf;
    stream->s_config.mem_vtbl.put_bufs = mm_app_stream_deinitbuf;
    stream->s_config.mem_vtbl.clean_invalidate_buf =
      mm_app_stream_clean_invalidate_buf;
    stream->s_config.mem_vtbl.invalidate_buf = mm_app_stream_invalidate_buf;
    stream->s_config.mem_vtbl.user_data = (void *)stream;
    stream->s_config.stream_cb = stream_cb;
    stream->s_config.userdata = userdata;
    stream->num_of_bufs = num_bufs;

    stream->s_config.stream_info = (cam_stream_info_t *)stream->s_info_buf.buf.buffer;
    memset(stream->s_config.stream_info, 0, sizeof(cam_stream_info_t));
    stream->s_config.stream_info->stream_type = CAM_STREAM_TYPE_METADATA;
    stream->s_config.stream_info->streaming_mode = CAM_STREAMING_MODE_CONTINUOUS;
    stream->s_config.stream_info->fmt = DEFAULT_PREVIEW_FORMAT;
    stream->s_config.stream_info->dim.width = sizeof(cam_metadata_info_t);
    stream->s_config.stream_info->dim.height = 1;
    stream->s_config.padding_info = cam_cap->padding_info;

    rc = mm_app_config_stream(test_obj, channel, stream, &stream->s_config);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:config preview stream err=%d\n", __func__, rc);
        return NULL;
    }

    return stream;
}

mm_camera_stream_t * mm_app_add_preview_stream(mm_camera_test_obj_t *test_obj,
                                               mm_camera_channel_t *channel,
                                               mm_camera_buf_notify_t stream_cb,
                                               void *userdata,
                                               uint8_t num_bufs)
{
    int rc = MM_CAMERA_OK;
    mm_camera_stream_t *stream = NULL;
    cam_capability_t *cam_cap = (cam_capability_t *)(test_obj->cap_buf.buf.buffer);

    stream = mm_app_add_stream(test_obj, channel);
    if (NULL == stream) {
        CDBG_ERROR("%s: add stream failed\n", __func__);
        return NULL;
    }
    stream->s_config.mem_vtbl.get_bufs = mm_app_stream_initbuf;
    stream->s_config.mem_vtbl.put_bufs = mm_app_stream_deinitbuf;
    stream->s_config.mem_vtbl.clean_invalidate_buf =
      mm_app_stream_clean_invalidate_buf;
    stream->s_config.mem_vtbl.invalidate_buf = mm_app_stream_invalidate_buf;
    stream->s_config.mem_vtbl.user_data = (void *)stream;
    stream->s_config.stream_cb = stream_cb;
    stream->s_config.userdata = userdata;
    stream->num_of_bufs = num_bufs;

    stream->s_config.stream_info = (cam_stream_info_t *)stream->s_info_buf.buf.buffer;
    memset(stream->s_config.stream_info, 0, sizeof(cam_stream_info_t));
    stream->s_config.stream_info->stream_type = CAM_STREAM_TYPE_PREVIEW;
    stream->s_config.stream_info->streaming_mode = CAM_STREAMING_MODE_CONTINUOUS;
    stream->s_config.stream_info->fmt = DEFAULT_PREVIEW_FORMAT;

    if ( test_obj->buffer_width == 0 || test_obj->buffer_height == 0 ) {
      stream->s_config.stream_info->dim.width = DEFAULT_PREVIEW_WIDTH;
      stream->s_config.stream_info->dim.height = DEFAULT_PREVIEW_HEIGHT;
    } else {
        stream->s_config.stream_info->dim.width = test_obj->buffer_width;
        stream->s_config.stream_info->dim.height = test_obj->buffer_height;
    }
    stream->s_config.padding_info = cam_cap->padding_info;

    rc = mm_app_config_stream(test_obj, channel, stream, &stream->s_config);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:config preview stream err=%d\n", __func__, rc);
        return NULL;
    }

    return stream;
}

mm_camera_stream_t * mm_app_add_raw_stream(mm_camera_test_obj_t *test_obj,
                                                mm_camera_channel_t *channel,
                                                mm_camera_buf_notify_t stream_cb,
                                                void *userdata,
                                                uint8_t num_bufs,
                                                uint8_t num_burst)
{
    int rc = MM_CAMERA_OK;
    mm_camera_stream_t *stream = NULL;
    cam_capability_t *cam_cap = (cam_capability_t *)(test_obj->cap_buf.buf.buffer);

    stream = mm_app_add_stream(test_obj, channel);
    if (NULL == stream) {
        CDBG_ERROR("%s: add stream failed\n", __func__);
        return NULL;
    }

    stream->s_config.mem_vtbl.get_bufs = mm_app_stream_initbuf;
    stream->s_config.mem_vtbl.put_bufs = mm_app_stream_deinitbuf;
    stream->s_config.mem_vtbl.invalidate_buf = mm_app_stream_invalidate_buf;
    stream->s_config.mem_vtbl.user_data = (void *)stream;
    stream->s_config.stream_cb = stream_cb;
    stream->s_config.userdata = userdata;
    stream->num_of_bufs = num_bufs;

    stream->s_config.stream_info = (cam_stream_info_t *)stream->s_info_buf.buf.buffer;
    memset(stream->s_config.stream_info, 0, sizeof(cam_stream_info_t));
    stream->s_config.stream_info->stream_type = CAM_STREAM_TYPE_RAW;
    if (num_burst == 0) {
        stream->s_config.stream_info->streaming_mode = CAM_STREAMING_MODE_CONTINUOUS;
    } else {
        stream->s_config.stream_info->streaming_mode = CAM_STREAMING_MODE_BURST;
        stream->s_config.stream_info->num_of_burst = num_burst;
    }
    stream->s_config.stream_info->fmt = test_obj->buffer_format;
    if ( test_obj->buffer_width == 0 || test_obj->buffer_height == 0 ) {
        stream->s_config.stream_info->dim.width = DEFAULT_SNAPSHOT_WIDTH;
        stream->s_config.stream_info->dim.height = DEFAULT_SNAPSHOT_HEIGHT;
    } else {
        stream->s_config.stream_info->dim.width = test_obj->buffer_width;
        stream->s_config.stream_info->dim.height = test_obj->buffer_height;
    }
    stream->s_config.padding_info = cam_cap->padding_info;

    rc = mm_app_config_stream(test_obj, channel, stream, &stream->s_config);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:config preview stream err=%d\n", __func__, rc);
        return NULL;
    }

    return stream;
}

mm_camera_stream_t * mm_app_add_snapshot_stream(mm_camera_test_obj_t *test_obj,
                                                mm_camera_channel_t *channel,
                                                mm_camera_buf_notify_t stream_cb,
                                                void *userdata,
                                                uint8_t num_bufs,
                                                uint8_t num_burst)
{
    int rc = MM_CAMERA_OK;
    mm_camera_stream_t *stream = NULL;
    cam_capability_t *cam_cap = (cam_capability_t *)(test_obj->cap_buf.buf.buffer);

    stream = mm_app_add_stream(test_obj, channel);
    if (NULL == stream) {
        CDBG_ERROR("%s: add stream failed\n", __func__);
        return NULL;
    }

    stream->s_config.mem_vtbl.get_bufs = mm_app_stream_initbuf;
    stream->s_config.mem_vtbl.put_bufs = mm_app_stream_deinitbuf;
    stream->s_config.mem_vtbl.clean_invalidate_buf =
      mm_app_stream_clean_invalidate_buf;
    stream->s_config.mem_vtbl.invalidate_buf = mm_app_stream_invalidate_buf;
    stream->s_config.mem_vtbl.user_data = (void *)stream;
    stream->s_config.stream_cb = stream_cb;
    stream->s_config.userdata = userdata;
    stream->num_of_bufs = num_bufs;

    stream->s_config.stream_info = (cam_stream_info_t *)stream->s_info_buf.buf.buffer;
    memset(stream->s_config.stream_info, 0, sizeof(cam_stream_info_t));
    stream->s_config.stream_info->stream_type = CAM_STREAM_TYPE_SNAPSHOT;
    if (num_burst == 0) {
        stream->s_config.stream_info->streaming_mode = CAM_STREAMING_MODE_CONTINUOUS;
    } else {
        stream->s_config.stream_info->streaming_mode = CAM_STREAMING_MODE_BURST;
        stream->s_config.stream_info->num_of_burst = num_burst;
    }
    stream->s_config.stream_info->fmt = DEFAULT_SNAPSHOT_FORMAT;
    if ( test_obj->buffer_width == 0 || test_obj->buffer_height == 0 ) {
        stream->s_config.stream_info->dim.width = DEFAULT_SNAPSHOT_WIDTH;
        stream->s_config.stream_info->dim.height = DEFAULT_SNAPSHOT_HEIGHT;
    } else {
        stream->s_config.stream_info->dim.width = test_obj->buffer_width;
        stream->s_config.stream_info->dim.height = test_obj->buffer_height;
    }
    stream->s_config.padding_info = cam_cap->padding_info;

    rc = mm_app_config_stream(test_obj, channel, stream, &stream->s_config);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:config preview stream err=%d\n", __func__, rc);
        return NULL;
    }

    return stream;
}

mm_camera_channel_t * mm_app_add_preview_channel(mm_camera_test_obj_t *test_obj)
{
    mm_camera_channel_t *channel = NULL;
    mm_camera_stream_t *stream = NULL;

    channel = mm_app_add_channel(test_obj,
                                 MM_CHANNEL_TYPE_PREVIEW,
                                 NULL,
                                 NULL,
                                 NULL);
    if (NULL == channel) {
        CDBG_ERROR("%s: add channel failed", __func__);
        return NULL;
    }

    stream = mm_app_add_preview_stream(test_obj,
                                       channel,
                                       mm_app_preview_notify_cb,
                                       (void *)test_obj,
                                       PREVIEW_BUF_NUM);
    if (NULL == stream) {
        CDBG_ERROR("%s: add stream failed\n", __func__);
        mm_app_del_channel(test_obj, channel);
        return NULL;
    }

    return channel;
}

int mm_app_stop_and_del_channel(mm_camera_test_obj_t *test_obj,
                                mm_camera_channel_t *channel)
{
    int rc = MM_CAMERA_OK;
    mm_camera_stream_t *stream = NULL;
    uint8_t i;

    rc = mm_app_stop_channel(test_obj, channel);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:Stop Preview failed rc=%d\n", __func__, rc);
    }

    for (i = 0; i < channel->num_streams; i++) {
        stream = &channel->streams[i];
        rc = mm_app_del_stream(test_obj, channel, stream);
        if (MM_CAMERA_OK != rc) {
            CDBG_ERROR("%s:del stream(%d) failed rc=%d\n", __func__, i, rc);
        }
    }

    rc = mm_app_del_channel(test_obj, channel);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:delete channel failed rc=%d\n", __func__, rc);
    }

    return rc;
}

int mm_app_start_preview(mm_camera_test_obj_t *test_obj)
{
    int rc = MM_CAMERA_OK;
    mm_camera_channel_t *channel = NULL;
    mm_camera_stream_t *stream = NULL;
    uint8_t i;

    channel =  mm_app_add_preview_channel(test_obj);
    if (NULL == channel) {
        CDBG_ERROR("%s: add channel failed", __func__);
        return -MM_CAMERA_E_GENERAL;
    }

    rc = mm_app_start_channel(test_obj, channel);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:start preview failed rc=%d\n", __func__, rc);
        for (i = 0; i < channel->num_streams; i++) {
            stream = &channel->streams[i];
            mm_app_del_stream(test_obj, channel, stream);
        }
        mm_app_del_channel(test_obj, channel);
        return rc;
    }

    return rc;
}

int mm_app_stop_preview(mm_camera_test_obj_t *test_obj)
{
    int rc = MM_CAMERA_OK;

    mm_camera_channel_t *channel =
        mm_app_get_channel_by_type(test_obj, MM_CHANNEL_TYPE_PREVIEW);

    rc = mm_app_stop_and_del_channel(test_obj, channel);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:Stop Preview failed rc=%d\n", __func__, rc);
    }

    return rc;
}

int mm_app_start_preview_zsl(mm_camera_test_obj_t *test_obj)
{
    int32_t rc = MM_CAMERA_OK;
    mm_camera_channel_t *channel = NULL;
    mm_camera_stream_t *s_preview = NULL;
    mm_camera_stream_t *s_metadata = NULL;
    mm_camera_stream_t *s_main = NULL;
    mm_camera_channel_attr_t attr;

    memset(&attr, 0, sizeof(mm_camera_channel_attr_t));
    attr.notify_mode = MM_CAMERA_SUPER_BUF_NOTIFY_CONTINUOUS;
    attr.look_back = 2;
    attr.post_frame_skip = 0;
    attr.water_mark = 2;
    attr.max_unmatched_frames = 3;
    channel = mm_app_add_channel(test_obj,
                                 MM_CHANNEL_TYPE_ZSL,
                                 &attr,
                                 mm_app_zsl_notify_cb,
                                 test_obj);
    if (NULL == channel) {
        CDBG_ERROR("%s: add channel failed", __func__);
        return -MM_CAMERA_E_GENERAL;
    }

    s_preview = mm_app_add_preview_stream(test_obj,
                                          channel,
                                          mm_app_preview_notify_cb,
                                          (void *)test_obj,
                                          PREVIEW_BUF_NUM);
    if (NULL == s_preview) {
        CDBG_ERROR("%s: add preview stream failed\n", __func__);
        mm_app_del_channel(test_obj, channel);
        return rc;
    }

    s_metadata = mm_app_add_metadata_stream(test_obj,
                                            channel,
                                            mm_app_metadata_notify_cb,
                                            (void *)test_obj,
                                            PREVIEW_BUF_NUM);
    if (NULL == s_metadata) {
        CDBG_ERROR("%s: add metadata stream failed\n", __func__);
        mm_app_del_channel(test_obj, channel);
        return rc;
    }

    s_main = mm_app_add_snapshot_stream(test_obj,
                                        channel,
                                        NULL,
                                        NULL,
                                        PREVIEW_BUF_NUM,
                                        0);
    if (NULL == s_main) {
        CDBG_ERROR("%s: add main snapshot stream failed\n", __func__);
        mm_app_del_stream(test_obj, channel, s_preview);
        mm_app_del_channel(test_obj, channel);
        return rc;
    }

    rc = mm_app_start_channel(test_obj, channel);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:start zsl failed rc=%d\n", __func__, rc);
        mm_app_del_stream(test_obj, channel, s_preview);
        mm_app_del_stream(test_obj, channel, s_metadata);
        mm_app_del_stream(test_obj, channel, s_main);
        mm_app_del_channel(test_obj, channel);
        return rc;
    }

    return rc;
}

int mm_app_stop_preview_zsl(mm_camera_test_obj_t *test_obj)
{
    int rc = MM_CAMERA_OK;

    mm_camera_channel_t *channel =
        mm_app_get_channel_by_type(test_obj, MM_CHANNEL_TYPE_ZSL);

    rc = mm_app_stop_and_del_channel(test_obj, channel);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:Stop Preview failed rc=%d\n", __func__, rc);
    }

    return rc;
}

int mm_app_initialize_fb(mm_camera_test_obj_t *test_obj)
{
    int rc = MM_CAMERA_OK;

    assert( ( NULL != test_obj ) && ( 0 == test_obj->fb_fd ) );

    test_obj->fb_fd = open(FB_PATH, O_RDWR);
    if ( 0 > test_obj->fb_fd ) {
        CDBG_ERROR("%s: FB device open failed rc=%d, %s\n",
                   __func__,
                   -errno,
                   strerror(errno));
        rc = -errno;
        goto FAIL;
    }

    rc = ioctl(test_obj->fb_fd, FBIOGET_VSCREENINFO, &test_obj->vinfo);
    if ( MM_CAMERA_OK != rc ) {
        CDBG_ERROR("%s: Can not retrieve screen info rc=%d, %s\n",
                   __func__,
                   -errno,
                   strerror(errno));
        rc = -errno;
        goto FAIL;
    }

    if ( ( 0 == test_obj->vinfo.yres_virtual ) ||
         ( 0 == test_obj->vinfo.yres ) ||
         ( test_obj->vinfo.yres > test_obj->vinfo.yres_virtual ) ) {
        CDBG_ERROR("%s: Invalid FB virtual yres: %d, yres: %d\n",
                   __func__,
                   test_obj->vinfo.yres_virtual,
                   test_obj->vinfo.yres);
        rc = MM_CAMERA_E_GENERAL;
        goto FAIL;
    }

    if ( ( 0 == test_obj->vinfo.xres_virtual ) ||
         ( 0 == test_obj->vinfo.xres ) ||
         ( test_obj->vinfo.xres > test_obj->vinfo.xres_virtual ) ) {
        CDBG_ERROR("%s: Invalid FB virtual xres: %d, xres: %d\n",
                   __func__,
                   test_obj->vinfo.xres_virtual,
                   test_obj->vinfo.xres);
        rc = MM_CAMERA_E_GENERAL;
        goto FAIL;
    }

    test_obj->frame_count = test_obj->vinfo.yres_virtual / test_obj->vinfo.yres;
    test_obj->slice_size = test_obj->vinfo.xres * ( test_obj->vinfo.yres - 1 ) * DEFAULT_OV_FORMAT_BPP;
    test_obj->frame_size = test_obj->slice_size + test_obj->vinfo.xres * DEFAULT_OV_FORMAT_BPP;

    if ( test_obj->buffer_format == CAM_FORMAT_YUV_420_NV21 ) {
        test_obj->buffer_size = test_obj->buffer_width * test_obj->buffer_height * DEFAULT_CAMERA_FORMAT_BPP;
    } else if ( test_obj->buffer_format == CAM_FORMAT_BAYER_QCOM_RAW_10BPP_GBRG ) {
        test_obj->buffer_size = (test_obj->buffer_width + 11)/12 * 12;
        test_obj->buffer_size = test_obj->buffer_size * test_obj->buffer_height * 8 / 6;
    } else {
        CDBG_ERROR(" %s : Unsupported buffer format %d\n",
                   __func__,
                   test_obj->buffer_format);
        rc = MM_CAMERA_E_GENERAL;
        goto FAIL;
    }

    test_obj->slice_count = test_obj->buffer_size / test_obj->slice_size;
    if ( MAX_SLICES < test_obj->slice_count ) {
        CDBG_ERROR("%s: Too many slices %d\n",
                   __func__,
                   test_obj->slice_count);
        rc = MM_CAMERA_E_GENERAL;
        goto FAIL;
    }

    memset(&test_obj->data_overlay, 0, sizeof(struct mdp_overlay));
    if ( 0 == test_obj->slice_count ) {
        size_t dst_height = test_obj->buffer_size / (test_obj->vinfo.xres * DEFAULT_OV_FORMAT_BPP);
        test_obj->data_overlay.src.width  = test_obj->vinfo.xres;
        test_obj->data_overlay.src.height = dst_height;
        test_obj->data_overlay.src_rect.w = test_obj->vinfo.xres;
        test_obj->data_overlay.src_rect.h = dst_height;
        test_obj->data_overlay.dst_rect.w = test_obj->vinfo.xres;
        test_obj->data_overlay.dst_rect.h = dst_height;
    } else {
        test_obj->data_overlay.src.width  = test_obj->vinfo.xres;
        test_obj->data_overlay.src.height = test_obj->vinfo.yres - 1;
        test_obj->data_overlay.src_rect.w = test_obj->vinfo.xres;
        test_obj->data_overlay.src_rect.h = test_obj->vinfo.yres - 1;
        test_obj->data_overlay.dst_rect.w = test_obj->vinfo.xres;
        test_obj->data_overlay.dst_rect.h = test_obj->vinfo.yres - 1;
    }
    test_obj->data_overlay.src.format = DEFAULT_OV_FORMAT;
    test_obj->data_overlay.src_rect.x = 0;
    test_obj->data_overlay.src_rect.y = 0;
    test_obj->data_overlay.dst_rect.x = 0;
    test_obj->data_overlay.dst_rect.y = 0;
    test_obj->data_overlay.z_order = 1;

    test_obj->fb_base = mmap(0,
                             test_obj->frame_size * test_obj->frame_count,
                             PROT_WRITE,
                             MAP_SHARED,
                             test_obj->fb_fd,
                             0);
    if ( MAP_FAILED  == test_obj->fb_base ) {
        CDBG_ERROR("%s: ( Error while memory mapping frame buffer %s",
                   __func__,
                   strerror(errno));
        rc = -errno;
        goto FAIL;
    }

    CDBG("%s: FB initialized fd: %d fd_base: %p, frame_count: %d, slice_size: %d, frame_size: %d slice_count: %d, buffer_size: %d\n",
               __func__,
               test_obj->fb_fd,
               test_obj->fb_base,
               test_obj->frame_count,
               test_obj->slice_size,
               test_obj->frame_size,
               test_obj->slice_count,
               test_obj->buffer_size);

    test_obj->data_overlay.id = MSMFB_NEW_REQUEST;
    rc = ioctl(test_obj->fb_fd, MSMFB_OVERLAY_SET, &test_obj->data_overlay);
    if (rc < 0) {
        CDBG_ERROR("%s : MSMFB_OVERLAY_SET failed! err=%d\n",
                   __func__,
                   test_obj->data_overlay.id);
        return MM_CAMERA_E_GENERAL;
    }
    CDBG("%s: Overlay set with overlay id: %d", __func__, test_obj->data_overlay.id);

    memset(&test_obj->marker_overlay, 0, sizeof(struct mdp_overlay));
    test_obj->marker_overlay.src.width  = test_obj->vinfo.xres;
    test_obj->marker_overlay.src.height = MARKER_HEIGHT;
    test_obj->marker_overlay.src.format = DEFAULT_OV_FORMAT;
    test_obj->marker_overlay.src_rect.x = 0;
    test_obj->marker_overlay.src_rect.y = 0;
    test_obj->marker_overlay.src_rect.w = test_obj->vinfo.xres;
    test_obj->marker_overlay.src_rect.h = MARKER_HEIGHT;
    test_obj->marker_overlay.dst_rect.x = 0;
    test_obj->marker_overlay.dst_rect.y = test_obj->vinfo.yres - MARKER_HEIGHT;
    test_obj->marker_overlay.dst_rect.w = test_obj->vinfo.xres;
    test_obj->marker_overlay.dst_rect.h = MARKER_HEIGHT;
    test_obj->marker_overlay.z_order = 0;

    test_obj->marker_overlay.id = MSMFB_NEW_REQUEST;
    rc = ioctl(test_obj->fb_fd, MSMFB_OVERLAY_SET, &test_obj->marker_overlay);
    if (rc < 0) {
        CDBG_ERROR("%s : MSMFB_OVERLAY_SET failed! err=%d\n",
                   __func__,
                   test_obj->marker_overlay.id);
        return MM_CAMERA_E_GENERAL;
    }
    CDBG("%s: Marker overlay set with overlay id: %d", __func__, test_obj->marker_overlay.id);

    test_obj->marker_buffer.mem_info.size = test_obj->vinfo.xres*MARKER_HEIGHT*DEFAULT_OV_FORMAT_BPP;
    rc = mm_app_allocate_ion_memory(&test_obj->marker_buffer, 0x1 << CAMERA_ION_HEAP_ID);
    if ( MM_CAMERA_OK != rc ) {
        CDBG_ERROR("%s : Marker buffer allocation failed %d", __func__, rc);
        goto FAIL;
    }

    return rc;

FAIL:

    if ( 0 < test_obj->fb_fd ) {
        close(test_obj->fb_fd);
    }

    return rc;
}

int mm_app_close_fb(mm_camera_test_obj_t *test_obj)
{
    int rc = MM_CAMERA_OK;

    assert( ( NULL != test_obj ) && ( 0 < test_obj->fb_fd ) );

    if (ioctl(test_obj->fb_fd, MSMFB_OVERLAY_UNSET, &test_obj->data_overlay.id)) {
        CDBG_ERROR("\nERROR! MSMFB_OVERLAY_UNSET failed! (Line %d)\n", __LINE__);
    }

    if (ioctl(test_obj->fb_fd, MSMFB_OVERLAY_UNSET, &test_obj->marker_overlay.id)) {
        CDBG_ERROR("\nERROR! MSMFB_OVERLAY_UNSET failed! (Line %d)\n", __LINE__);
    }

    if (ioctl(test_obj->fb_fd, FBIOPAN_DISPLAY, &test_obj->vinfo) < 0) {
        CDBG_ERROR("ERROR: FBIOPAN_DISPLAY failed! line=%d\n", __LINE__);
    }

    munmap(test_obj->fb_base, test_obj->frame_size * test_obj->frame_count);
    test_obj->fb_base = NULL;
    close(test_obj->fb_fd);
    test_obj->fb_fd = 0;

    mm_app_deallocate_ion_memory(&test_obj->marker_buffer);

    return rc;
}

void memset16(void *pDst, uint16_t value, int count)
{
    uint16_t *ptr = pDst;
    while (count--)
        *ptr++ = value;
}

int mm_app_overlay_display(mm_camera_test_obj_t *test_obj, int bufferFd)
{
    int rc = MM_CAMERA_OK;
    struct msmfb_overlay_data ovdata;
    size_t current_slice = 0;
    uint8_t slice = SLICE_BASE;
    static uint8_t counter = 0;
    uint16_t marker_value = 0;

    do {

        ovdata.id = test_obj->data_overlay.id;
        ovdata.data.flags = 0;
        ovdata.data.offset = test_obj->slice_size*current_slice;
        ovdata.data.memory_id = bufferFd;

        current_slice++;
        slice = SLICE_BASE * current_slice;
        marker_value = ( counter << 8 ) | slice;
        memset16(test_obj->marker_buffer.mem_info.data,
                 marker_value,
                 test_obj->marker_buffer.mem_info.size/2);

        if (ioctl(test_obj->fb_fd, MSMFB_OVERLAY_PLAY, &ovdata)) {
            CDBG_ERROR("%s : MSMFB_OVERLAY_PLAY failed!", __func__);
            return MM_CAMERA_E_GENERAL;
        }

        ovdata.id = test_obj->marker_overlay.id;
        ovdata.data.flags = 0;
        ovdata.data.offset = 0;
        ovdata.data.memory_id = test_obj->marker_buffer.mem_info.fd;

        if (ioctl(test_obj->fb_fd, MSMFB_OVERLAY_PLAY, &ovdata)) {
            CDBG_ERROR("%s : MSMFB_OVERLAY_PLAY failed!", __func__);
            return MM_CAMERA_E_GENERAL;
        }

        if (ioctl(test_obj->fb_fd, FBIOPAN_DISPLAY, &test_obj->vinfo) < 0) {
            CDBG_ERROR("%s : FBIOPAN_DISPLAY failed!", __func__);
            return MM_CAMERA_E_GENERAL;
        }

        CDBG("Bufffer %d slice %d, offset %d queued in overlay with marker 0x%x",
                   bufferFd,
                   (current_slice- 1),
                   test_obj->slice_size*current_slice,
                   marker_value);

    } while ( current_slice <= test_obj->slice_count );
    counter++;

    return rc;
}

int mm_app_fb_write(mm_camera_test_obj_t *test_obj, char *buffer)
{
    size_t current_slice = 0;
    uint8_t slice = SLICE_BASE;
    int rc = MM_CAMERA_OK;
    static uint8_t counter = 0;
    int current_frame = 0;
    char *dst = NULL, *src = NULL;

    assert( ( NULL != test_obj ) &&
            ( 0 < test_obj->fb_fd ) &&
            ( NULL != test_obj->fb_base) &&
            ( NULL != buffer ) );

    do {
        current_slice++;
        slice *= current_slice;

        test_obj->vinfo.yoffset = test_obj->vinfo.yres * current_frame;
        dst = ( char * ) test_obj->fb_base + test_obj->frame_size * current_frame;
        src =  buffer + test_obj->slice_size * current_frame;
        memcpy(dst, src, test_obj->slice_size);
        *((uint8_t *)test_obj->fb_base + test_obj->frame_size * current_frame - 2) = counter;
        *((uint8_t *)test_obj->fb_base + test_obj->frame_size * current_frame - 1) = slice;
        ioctl(test_obj->fb_fd, FBIOPAN_DISPLAY, &test_obj->vinfo);

        counter++;
        current_frame++;
        current_frame %= test_obj->frame_count;
    } while ( current_slice < test_obj->slice_count );

    return rc;
}
