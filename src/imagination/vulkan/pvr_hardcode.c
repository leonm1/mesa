/*
 * Copyright © 2022 Imagination Technologies Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <vulkan/vulkan_core.h>

#include "hwdef/rogue_hw_utils.h"
#include "pvr_hardcode.h"
#include "pvr_private.h"
#include "rogue/rogue.h"
#include "usc/hardcoded_apps/pvr_simple_compute.h"
#include "util/macros.h"
#include "util/u_process.h"

/**
 * \file pvr_hardcode.c
 *
 * \brief Contains hard coding functions.
 * This should eventually be deleted as the compiler becomes more capable.
 */

enum pvr_hard_code_shader_type {
   PVR_HARD_CODE_SHADER_TYPE_COMPUTE,
   PVR_HARD_CODE_SHADER_TYPE_GRAPHICS,
};

/* Applications for which the compiler is capable of generating valid shaders.
 */
static const char *const compilable_progs[] = {
   "triangle",
};

static const struct pvr_hard_coding_data {
   const char *const name;
   enum pvr_hard_code_shader_type type;

   union {
      struct {
         const uint8_t *const shader;
         size_t shader_size;

         /* Note that the bo field will be unused. */
         const struct pvr_compute_pipeline_shader_state shader_info;

         const struct pvr_hard_code_compute_build_info build_info;
      } compute;

      struct {
         struct rogue_shader_binary *const *const vert_shaders;
         struct rogue_shader_binary *const *const frag_shaders;

         const struct pvr_vertex_shader_state *const *const vert_shader_states;
         const struct pvr_fragment_shader_state *const *const frag_shader_states;

         const struct pvr_hard_code_graphics_build_info *const
            *const build_infos;

         uint32_t shader_count;
      } graphics;
   };

} hard_coding_table[] = {
   {
      .name = "simple-compute",
      .type = PVR_HARD_CODE_SHADER_TYPE_COMPUTE,

      .compute = {
         .shader = pvr_simple_compute_shader,
         .shader_size = sizeof(pvr_simple_compute_shader),

         .shader_info = {
            .uses_atomic_ops = false,
            .uses_barrier = false,
            .uses_num_workgroups = false,

            .const_shared_reg_count = 4,
            .input_register_count = 8,
            .work_size = 1 * 1 * 1,
            .coefficient_register_count = 4,
         },

         .build_info = {
            .ubo_data = { 0 },

            .local_invocation_regs = { 0, 1 },
            .work_group_regs = { 0, 1, 2 },
            .barrier_reg = ROGUE_REG_UNUSED,
            .usc_temps = 0,

            .explicit_conts_usage = {
               .start_offset = 0,
            },
         },
      }
   },
};

bool pvr_hard_code_shader_required(void)
{
   const char *const program = util_get_process_name();

   for (uint32_t i = 0; i < ARRAY_SIZE(compilable_progs); i++) {
      if (strcmp(program, compilable_progs[i]) == 0)
         return false;
   }

   return true;
}

static const struct pvr_hard_coding_data *pvr_get_hard_coding_data()
{
   const char *const program = util_get_process_name();

   for (uint32_t i = 0; i < ARRAY_SIZE(hard_coding_table); i++) {
      if (strcmp(program, hard_coding_table[i].name) == 0)
         return &hard_coding_table[i];
   }

   mesa_loge("Could not find hard coding data for %s", program);

   return NULL;
}

VkResult pvr_hard_code_compute_pipeline(
   struct pvr_device *const device,
   struct pvr_compute_pipeline_shader_state *const shader_state_out,
   struct pvr_hard_code_compute_build_info *const build_info_out)
{
   const uint32_t cache_line_size =
      rogue_get_slc_cache_line_size(&device->pdevice->dev_info);
   const struct pvr_hard_coding_data *const data = pvr_get_hard_coding_data();

   assert(data->type == PVR_HARD_CODE_SHADER_TYPE_COMPUTE);

   mesa_logd("Hard coding compute pipeline for %s", data->name);

   *build_info_out = data->compute.build_info;
   *shader_state_out = data->compute.shader_info;

   return pvr_gpu_upload_usc(device,
                             data->compute.shader,
                             data->compute.shader_size,
                             cache_line_size,
                             &shader_state_out->bo);
}

void pvr_hard_code_graphics_shaders(
   uint32_t pipeline_n,
   struct rogue_shader_binary **const vert_shader_out,
   struct rogue_shader_binary **const frag_shader_out)
{
   const struct pvr_hard_coding_data *const data = pvr_get_hard_coding_data();

   assert(data->type == PVR_HARD_CODE_SHADER_TYPE_GRAPHICS);
   assert(pipeline_n < data->graphics.shader_count);

   mesa_logd("Hard coding graphics pipeline for %s", data->name);

   *vert_shader_out = data->graphics.vert_shaders[pipeline_n];
   *frag_shader_out = data->graphics.frag_shaders[pipeline_n];
}

void pvr_hard_code_graphics_vertex_state(
   uint32_t pipeline_n,
   struct pvr_vertex_shader_state *const vert_state_out)
{
   const struct pvr_hard_coding_data *const data = pvr_get_hard_coding_data();

   assert(data->type == PVR_HARD_CODE_SHADER_TYPE_GRAPHICS);
   assert(pipeline_n < data->graphics.shader_count);

   *vert_state_out = *data->graphics.vert_shader_states[0];
}

void pvr_hard_code_graphics_fragment_state(
   uint32_t pipeline_n,
   struct pvr_fragment_shader_state *const frag_state_out)
{
   const struct pvr_hard_coding_data *const data = pvr_get_hard_coding_data();

   assert(data->type == PVR_HARD_CODE_SHADER_TYPE_GRAPHICS);
   assert(pipeline_n < data->graphics.shader_count);

   *frag_state_out = *data->graphics.frag_shader_states[0];
}

void pvr_hard_code_graphics_inject_build_info(
   uint32_t pipeline_n,
   struct rogue_build_ctx *ctx,
   struct pvr_explicit_constant_usage *const vert_common_data_out,
   struct pvr_explicit_constant_usage *const frag_common_data_out)
{
   const struct pvr_hard_coding_data *const data = pvr_get_hard_coding_data();

   assert(data->type == PVR_HARD_CODE_SHADER_TYPE_GRAPHICS);
   assert(pipeline_n < data->graphics.shader_count);

   ctx->stage_data = data->graphics.build_infos[pipeline_n]->stage_data;
   ctx->common_data[MESA_SHADER_VERTEX] =
      data->graphics.build_infos[pipeline_n]->vert_common_data;
   ctx->common_data[MESA_SHADER_FRAGMENT] =
      data->graphics.build_infos[pipeline_n]->frag_common_data;

   assert(
      ctx->common_data[MESA_SHADER_VERTEX].temps ==
      data->graphics.vert_shader_states[pipeline_n]->stage_state.temps_count);
   assert(
      ctx->common_data[MESA_SHADER_FRAGMENT].temps ==
      data->graphics.frag_shader_states[pipeline_n]->stage_state.temps_count);

   assert(ctx->common_data[MESA_SHADER_VERTEX].coeffs ==
          data->graphics.vert_shader_states[pipeline_n]
             ->stage_state.coefficient_size);
   assert(ctx->common_data[MESA_SHADER_FRAGMENT].coeffs ==
          data->graphics.frag_shader_states[pipeline_n]
             ->stage_state.coefficient_size);

   *vert_common_data_out =
      data->graphics.build_infos[pipeline_n]->vert_explicit_conts_usage;
   *frag_common_data_out =
      data->graphics.build_infos[pipeline_n]->frag_explicit_conts_usage;
}