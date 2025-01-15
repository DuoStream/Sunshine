/**
 * @file src/platform/windows/display_ram.cpp
 * @brief Definitions for handling ram.
 */
#include "display.h"

#include "misc.h"
#include "src/logging.h"

namespace platf {
  using namespace std::literals;
}

namespace platf::dxgi {
  struct img_t: public ::platf::img_t {
    ~img_t() override {
      delete[] data;
      data = nullptr;
    }
  };

  void
  blend_cursor_monochrome(const cursor_t &cursor, img_t &img) {
    int height = cursor.shape_info.Height / 2;
    int width = cursor.shape_info.Width;
    int pitch = cursor.shape_info.Pitch;

    // img cursor.{x,y} < 0, skip parts of the cursor.img_data
    auto cursor_skip_y = -std::min(0, cursor.y);
    auto cursor_skip_x = -std::min(0, cursor.x);

    // img cursor.{x,y} > img.{x,y}, truncate parts of the cursor.img_data
    auto cursor_truncate_y = std::max(0, cursor.y - img.height);
    auto cursor_truncate_x = std::max(0, cursor.x - img.width);

    auto cursor_width = width - cursor_skip_x - cursor_truncate_x;
    auto cursor_height = height - cursor_skip_y - cursor_truncate_y;

    if (cursor_height > height || cursor_width > width) {
      return;
    }

    auto img_skip_y = std::max(0, cursor.y);
    auto img_skip_x = std::max(0, cursor.x);

    auto cursor_img_data = cursor.img_data.data() + cursor_skip_y * pitch;

    int delta_height = std::min(cursor_height - cursor_truncate_y, std::max(0, img.height - img_skip_y));
    int delta_width = std::min(cursor_width - cursor_truncate_x, std::max(0, img.width - img_skip_x));

    auto pixels_per_byte = width / pitch;
    auto bytes_per_row = delta_width / pixels_per_byte;

    auto img_data = (int *) img.data;
    for (int i = 0; i < delta_height; ++i) {
      auto and_mask = &cursor_img_data[i * pitch];
      auto xor_mask = &cursor_img_data[(i + height) * pitch];

      auto img_pixel_p = &img_data[(i + img_skip_y) * (img.row_pitch / img.pixel_pitch) + img_skip_x];

      auto skip_x = cursor_skip_x;
      for (int x = 0; x < bytes_per_row; ++x) {
        for (auto bit = 0u; bit < 8; ++bit) {
          if (skip_x > 0) {
            --skip_x;

            continue;
          }

          int and_ = *and_mask & (1 << (7 - bit)) ? -1 : 0;
          int xor_ = *xor_mask & (1 << (7 - bit)) ? -1 : 0;

          *img_pixel_p &= and_;
          *img_pixel_p ^= xor_;

          ++img_pixel_p;
        }

        ++and_mask;
        ++xor_mask;
      }
    }
  }

  void
  apply_color_alpha(int *img_pixel_p, int cursor_pixel) {
    auto colors_out = (std::uint8_t *) &cursor_pixel;
    auto colors_in = (std::uint8_t *) img_pixel_p;

    // TODO: When use of IDXGIOutput5 is implemented, support different color formats
    auto alpha = colors_out[3];
    if (alpha == 255) {
      *img_pixel_p = cursor_pixel;
    }
    else {
      colors_in[0] = colors_out[0] + (colors_in[0] * (255 - alpha) + 255 / 2) / 255;
      colors_in[1] = colors_out[1] + (colors_in[1] * (255 - alpha) + 255 / 2) / 255;
      colors_in[2] = colors_out[2] + (colors_in[2] * (255 - alpha) + 255 / 2) / 255;
    }
  }

  void
  apply_color_masked(int *img_pixel_p, int cursor_pixel) {
    // TODO: When use of IDXGIOutput5 is implemented, support different color formats
    auto alpha = ((std::uint8_t *) &cursor_pixel)[3];
    if (alpha == 0xFF) {
      *img_pixel_p ^= cursor_pixel;
    }
    else {
      *img_pixel_p = cursor_pixel;
    }
  }

  void
  blend_cursor_color(const cursor_t &cursor, img_t &img, const bool masked) {
    int height = cursor.shape_info.Height;
    int width = cursor.shape_info.Width;
    int pitch = cursor.shape_info.Pitch;

    // img cursor.y < 0, skip parts of the cursor.img_data
    auto cursor_skip_y = -std::min(0, cursor.y);
    auto cursor_skip_x = -std::min(0, cursor.x);

    // img cursor.{x,y} > img.{x,y}, truncate parts of the cursor.img_data
    auto cursor_truncate_y = std::max(0, cursor.y - img.height);
    auto cursor_truncate_x = std::max(0, cursor.x - img.width);

    auto img_skip_y = std::max(0, cursor.y);
    auto img_skip_x = std::max(0, cursor.x);

    auto cursor_width = width - cursor_skip_x - cursor_truncate_x;
    auto cursor_height = height - cursor_skip_y - cursor_truncate_y;

    if (cursor_height > height || cursor_width > width) {
      return;
    }

    auto cursor_img_data = (int *) &cursor.img_data[cursor_skip_y * pitch];

    int delta_height = std::min(cursor_height - cursor_truncate_y, std::max(0, img.height - img_skip_y));
    int delta_width = std::min(cursor_width - cursor_truncate_x, std::max(0, img.width - img_skip_x));

    auto img_data = (int *) img.data;

    for (int i = 0; i < delta_height; ++i) {
      auto cursor_begin = &cursor_img_data[i * cursor.shape_info.Width + cursor_skip_x];
      auto cursor_end = &cursor_begin[delta_width];

      auto img_pixel_p = &img_data[(i + img_skip_y) * (img.row_pitch / img.pixel_pitch) + img_skip_x];
      std::for_each(cursor_begin, cursor_end, [&](int cursor_pixel) {
        if (masked) {
          apply_color_masked(img_pixel_p, cursor_pixel);
        }
        else {
          apply_color_alpha(img_pixel_p, cursor_pixel);
        }
        ++img_pixel_p;
      });
    }
  }

  void
  blend_cursor(const cursor_t &cursor, img_t &img) {
    switch (cursor.shape_info.Type) {
      case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR:
        blend_cursor_color(cursor, img, false);
        break;
      case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME:
        blend_cursor_monochrome(cursor, img);
        break;
      case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MASKED_COLOR:
        blend_cursor_color(cursor, img, true);
        break;
      default:
        BOOST_LOG(warning) << "Unsupported cursor format ["sv << cursor.shape_info.Type << ']';
    }
  }

  capture_e
  display_ram_t::snapshot(const pull_free_image_cb_t &pull_free_image_cb, std::shared_ptr<platf::img_t> &img_out, std::chrono::milliseconds timeout, bool cursor_visible) {
    // Get the shared framebuffer texture
    ID3D11Resource* iddTexture = NULL;
    capture_e result = dup.iddblt(device.get(), &iddTexture);
    if (result == capture_e::ok) {
      // We haven't determined the capture format yet
      if (capture_format == DXGI_FORMAT_UNKNOWN) {
        // Query the ID3D11Texture2D interface of the resource
        ID3D11Texture2D* iddTexture2D = nullptr;
        HRESULT hr = iddTexture->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&iddTexture2D));
        if (SUCCEEDED(hr))
        {
          // Get the texture description
          D3D11_TEXTURE2D_DESC desc;
          iddTexture2D->GetDesc(&desc);

          // And set the capture format
          capture_format = desc.Format;

          // Reduce the reference counter
          iddTexture2D->Release();
        }
      }

      // Get or create the staging texture
      ID3D11Resource* stagingTexture = texture.get();
      if (stagingTexture == NULL) {
        D3D11_TEXTURE2D_DESC t {};
        t.Width = width;
        t.Height = height;
        t.MipLevels = 1;
        t.ArraySize = 1;
        t.SampleDesc.Count = 1;
        t.Usage = D3D11_USAGE_STAGING;
        t.Format = capture_format;
        t.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        auto status = device->CreateTexture2D(&t, nullptr, &texture);
        if (SUCCEEDED(status)) {
          stagingTexture = texture.get();
        }
      }

      // We have a valid staging texture to work with
      if (stagingTexture != NULL) {
        // Copy the framebuffer
        device_ctx->CopyResource(stagingTexture, iddTexture);

        // Resume the swapchain
        dup.resumeSwapChain();

        // Unmap the previous frame's pixel buffer
        if(img_info.pData) {
          device_ctx->Unmap(texture.get(), 0);
          img_info.pData = nullptr;
        }

        // Map the current frame's pixel buffer
        HRESULT status = device_ctx->Map(texture.get(), 0, D3D11_MAP_READ, 0, &img_info);
        if(SUCCEEDED(status)) {
          // Get the next free image from the pool
          if (pull_free_image_cb(img_out)) {
            // Allocate the output image's pixel buffer
            if (complete_img((img_t *)img_out.get(), false) == 0) {
              // Copy the pixel buffer into the output image
              std::copy_n((std::uint8_t *)img_info.pData, height * img_info.RowPitch, (std::uint8_t *)img_out.get()->data);

              // Not the most accurate timestamp but better than none
              ((img_t *)img_out.get())->frame_timestamp = std::chrono::steady_clock::now();
            } else {
              result = capture_e::error;
            }
          } else {
            result = capture_e::interrupted;
          }

          // Unmap the current frame's pixel buffer
          if (img_info.pData == nullptr) {
            device_ctx->Unmap(texture.get(), 0);
            img_info.pData = nullptr;
          }
        } else {
          result = capture_e::error;
        }
      } else {
        result = capture_e::error;
      }
    }

    // Return the capture result
    return result;
  }

  std::shared_ptr<platf::img_t>
  display_ram_t::alloc_img() {
    auto img = std::make_shared<img_t>();

    // Initialize fields that are format-independent
    img->width = width;
    img->height = height;

    return img;
  }

  int
  display_ram_t::complete_img(platf::img_t *img, bool dummy) {
    // If this is not a dummy image, we must know the format by now
    if (!dummy && capture_format == DXGI_FORMAT_UNKNOWN) {
      BOOST_LOG(error) << "display_ram_t::complete_img() called with unknown capture format!";
      return -1;
    }

    img->pixel_pitch = get_pixel_pitch();

    if (dummy && !img->row_pitch) {
      // Assume our dummy image will have no padding
      img->row_pitch = img->pixel_pitch * img->width;
    }

    // Reallocate the image buffer if the pitch changes
    if (!dummy && img->row_pitch != img_info.RowPitch) {
      img->row_pitch = img_info.RowPitch;
      delete img->data;
      img->data = nullptr;
    }

    if (!img->data) {
      img->data = new std::uint8_t[img->row_pitch * height];
    }

    return 0;
  }

  int
  display_ram_t::dummy_img(platf::img_t *img) {
    if (complete_img(img, true)) {
      return -1;
    }

    std::fill_n((std::uint8_t *) img->data, height * img->row_pitch, 0);
    return 0;
  }

  std::vector<DXGI_FORMAT>
  display_ram_t::get_supported_capture_formats() {
    return { DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_B8G8R8X8_UNORM };
  }

  int
  display_ram_t::init(const ::video::config_t &config, const std::string &display_name) {
    if (display_base_t::init(config, display_name)) {
      return -1;
    }

    return 0;
  }

  std::unique_ptr<avcodec_encode_device_t>
  display_ram_t::make_avcodec_encode_device(pix_fmt_e pix_fmt) {
    return std::make_unique<avcodec_encode_device_t>();
  }

}  // namespace platf::dxgi
