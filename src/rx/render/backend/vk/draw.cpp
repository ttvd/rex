#include "draw.h"

#include "context.h"

#include "data_builder.h"

namespace rx::render::backend {
  
void detail_vk::frame_render::pre_clear(detail_vk::context& ctx_, const frontend::clear_command* clear) {
  
  clear->render_target->attachments().each_fwd([&](frontend::target::attachment& attachment) {
    texture* image =
      (attachment.kind == frontend::target::attachment::type::k_texture2D
      ? reinterpret_cast<texture*> (attachment.as_texture2D.texture + 1)
      : reinterpret_cast<texture*> (attachment.as_textureCM.texture + 1));
    image->add_use(ctx_, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, ctx_.graphics_index, true);
  });
  
}

void detail_vk::frame_render::clear(detail_vk::context& ctx_, const frontend::clear_command* clear) {
  
  clear->render_target->attachments().each_fwd([&](frontend::target::attachment& attachment) {
    if(attachment.kind == frontend::target::attachment::type::k_texture2D) {
      reinterpret_cast<texture*> (attachment.as_texture2D.texture + 1)->sync(ctx_, attachment.as_texture2D.texture, ctx_.graphics.get());
    } else {
      reinterpret_cast<texture*> (attachment.as_textureCM.texture + 1)->sync(ctx_, attachment.as_textureCM.texture, ctx_.graphics.get());
    }
  });
  
#if defined(RX_DEBUG)
  vk_log(log::level::k_verbose, "clear %s", ctx_.current_command->tag.description);
#endif
  
  // TODO : add a draw list and calculate renderpasses
  
}

void detail_vk::frame_render::pre_blit(detail_vk::context& ctx_, const frontend::blit_command* blit) {
  
  auto& src_texture = blit->src_target->attachments()[blit->src_attachment];
  auto src_tex = (src_texture.kind == frontend::target::attachment::type::k_textureCM ? reinterpret_cast<detail_vk::texture*>(src_texture.as_textureCM.texture + 1) : reinterpret_cast<detail_vk::texture*>(src_texture.as_texture2D.texture + 1));
  auto& dst_texture = blit->dst_target->attachments()[blit->dst_attachment];
  auto dst_tex = (dst_texture.kind == frontend::target::attachment::type::k_textureCM ? reinterpret_cast<detail_vk::texture*>(dst_texture.as_textureCM.texture + 1) : reinterpret_cast<detail_vk::texture*>(dst_texture.as_texture2D.texture + 1));
  
  src_tex->add_use(ctx_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, ctx_.graphics_index, false);
  dst_tex->add_use(ctx_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, ctx_.graphics_index, true);
  
}

void detail_vk::frame_render::blit(detail_vk::context& ctx_, const frontend::blit_command* blit) {
  
  auto& src_texture = blit->src_target->attachments()[blit->src_attachment];
  auto src_tex = (src_texture.kind == frontend::target::attachment::type::k_textureCM ? reinterpret_cast<detail_vk::texture*>(src_texture.as_textureCM.texture + 1) : reinterpret_cast<detail_vk::texture*>(src_texture.as_texture2D.texture + 1));
  uint32_t src_layer = (src_texture.kind == frontend::target::attachment::type::k_textureCM ? static_cast<uint8_t> (src_texture.as_textureCM.face) : 0);
  
  auto& dst_texture = blit->dst_target->attachments()[blit->dst_attachment];
  auto dst_tex = (dst_texture.kind == frontend::target::attachment::type::k_textureCM ? reinterpret_cast<detail_vk::texture*>(dst_texture.as_textureCM.texture + 1) : reinterpret_cast<detail_vk::texture*>(dst_texture.as_texture2D.texture + 1));
  uint32_t dst_layer = (src_texture.kind == frontend::target::attachment::type::k_textureCM ? static_cast<uint8_t> (src_texture.as_textureCM.face) : 0);
  
  VkCommandBuffer command = ctx_.graphics.get();
  
  src_tex->sync(ctx_,
                (src_texture.kind == frontend::target::attachment::type::k_textureCM ?
                reinterpret_cast<frontend::texture*>(src_texture.as_textureCM.texture) :
                reinterpret_cast<frontend::texture*>(src_texture.as_texture2D.texture)),
                command);
  
  dst_tex->sync(ctx_,
                (dst_texture.kind == frontend::target::attachment::type::k_textureCM ?
                reinterpret_cast<frontend::texture*>(dst_texture.as_textureCM.texture) :
                reinterpret_cast<frontend::texture*>(dst_texture.as_texture2D.texture)),
                command);
  
  VkImageBlit info {};
  info.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  info.srcSubresource.baseArrayLayer = src_layer;
  info.srcSubresource.layerCount = 1;
  info.srcSubresource.mipLevel = 0;
  
  info.srcOffsets[0].x = 0;
  info.srcOffsets[0].y = 0;
  info.srcOffsets[0].z = 0;
  info.srcOffsets[1].x = src_tex->extent.width;
  info.srcOffsets[1].y = src_tex->extent.height;
  info.srcOffsets[1].z = src_tex->extent.depth;
  
  info.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  info.dstSubresource.baseArrayLayer = dst_layer;
  info.dstSubresource.layerCount = 1;
  info.dstSubresource.mipLevel = 0;
  
  info.dstOffsets[0].x = 0;
  info.dstOffsets[0].y = 0;
  info.dstOffsets[0].z = 0;
  info.dstOffsets[1].x = dst_tex->extent.width;
  info.dstOffsets[1].y = dst_tex->extent.height;
  info.dstOffsets[1].z = dst_tex->extent.depth;
  
  vkCmdBlitImage(command, src_tex->handle, src_tex->current_layout, dst_tex->handle, dst_tex->current_layout, 1, &info, VK_FILTER_NEAREST);
  
#if defined(RX_DEBUG)
  vk_log(log::level::k_verbose, "blit %s", ctx_.current_command->tag.description);
#endif
  
}



void detail_vk::frame_render::pre_draw(detail_vk::context& ctx_, const frontend::draw_command* draw) {
  
  draw->render_target->attachments().each_fwd([&](frontend::target::attachment& attachment) {
    texture* image =
      (attachment.kind == frontend::target::attachment::type::k_texture2D
      ? reinterpret_cast<texture*> (attachment.as_texture2D.texture + 1)
      : reinterpret_cast<texture*> (attachment.as_textureCM.texture + 1));
    image->add_use(ctx_, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, ctx_.graphics_index, true);
  });
  
}

void detail_vk::frame_render::draw(detail_vk::context& ctx_, const frontend::draw_command* draw) {
  
  draw->render_target->attachments().each_fwd([&](frontend::target::attachment& attachment) {
    if(attachment.kind == frontend::target::attachment::type::k_texture2D) {
      reinterpret_cast<texture*> (attachment.as_texture2D.texture + 1)->sync(ctx_, attachment.as_texture2D.texture, ctx_.graphics.get());
    } else {
      reinterpret_cast<texture*> (attachment.as_textureCM.texture + 1)->sync(ctx_, attachment.as_textureCM.texture, ctx_.graphics.get());
    }
  });
  
#if defined(RX_DEBUG)
  vk_log(log::level::k_verbose, "draw %s", ctx_.current_command->tag.description);
#endif
  
}

}

