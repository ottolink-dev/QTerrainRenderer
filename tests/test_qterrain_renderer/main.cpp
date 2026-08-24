#include <filesystem>
#include <random>

#include <QApplication>

#include "qterrain_renderer.hpp"

int main(int argc, char *argv[])
{
  qtr::Logger::log()->info("testing qterrain_renderer...");

  qputenv("QT_LOGGING_RULES", QTR_QPUTENV_QT_LOGGING_RULES);
  QApplication app(argc, argv);

  qtr::RenderWidget *renderer = new qtr::RenderWidget("Test widget");

  renderer->show();

  // set the heightmap data
  {
    int                width, height;
    std::vector<float> data = qtr::load_png_as_grayscale("hmap.png", width, height);

    renderer->set_heightmap_geometry(data, width, height);
  }

  {
    int                  width, height;
    std::vector<uint8_t> data = qtr::load_png_as_8bit_rgba("texture.png", width, height);

    renderer->set_texture(qtr::keys::tex::albedo, data, width);
    // renderer->reset_texture(qtr::keys::tex::albedo);
  }

  {
    int                  width, height;
    std::vector<uint8_t> data = qtr::load_png_as_8bit_rgba("nmap2.png", width, height);

    renderer->set_texture(qtr::keys::tex::normal, data, width);
    renderer->reset_texture(qtr::keys::tex::normal);
  }

  // skybox
  {
    int         width, height;
    std::string sky_filename = "DaySkyHDRI057B_1K_TONEMAPPED.jpg";
    std::string sky_path = sky_filename;

    const std::vector<std::string> search_dirs = {".",
                                                  "data",
                                                  "../data",
                                                  "../../data",
                                                  "QTerrainRenderer/data",
                                                  "../QTerrainRenderer/data",
                                                  "../../QTerrainRenderer/data"};

    for (const auto &dir : search_dirs)
    {
      std::string candidate = dir + "/" + sky_filename;
      if (std::filesystem::exists(candidate))
      {
        sky_path = candidate;
        break;
      }
    }

    try
    {
      std::vector<uint8_t> data = qtr::load_image_as_8bit_rgba(sky_path, width, height);
      renderer->set_skybox_image(data, width);
    }
    catch (const std::exception &e)
    {
      qtr::Logger::log()->warn("Could not load skybox image {}: {}", sky_path, e.what());
    }
  }

  {
    renderer->makeCurrent();
    qtr::generate_plane(renderer->get_water_mesh(), 0.f, 0.1f * 0.4f, 0.f, 2.f, 2.f);
    renderer->doneCurrent();
  }

  {
    std::vector<float> x = {0.05f, 0.1f, 0.2f, 0.7f, 0.8f};
    std::vector<float> y = {0.2f, 0.2f, 0.4f, 0.7f, 0.8f};
    std::vector<float> h = {0.8f, 0.2f, 1.f, 0.5f, 0.7f};

    qtr::set_points(*renderer, x, y, h);
    renderer->reset_mesh(qtr::keys::mesh::points);
  }

  {
    std::vector<float> x = {0.05f, 0.1f, 0.2f, 0.7f, 0.8f};
    std::vector<float> y = {0.2f, 0.2f, 0.4f, 0.7f, 0.8f};
    std::vector<float> h = {0.8f, 0.2f, 1.f, 0.5f, 0.7f};

    qtr::set_path(*renderer, x, y, h);
    renderer->reset_mesh(qtr::keys::mesh::path);
  }

  if (false)
  {
    size_t             n = 50000;
    std::vector<float> x, y, h, r;

    for (size_t k = 0; k < n; ++k)
    {
      x.push_back((float)std::rand() / RAND_MAX);
      y.push_back((float)std::rand() / RAND_MAX);
      h.push_back(0.1f + 0.f * (float)std::rand() / RAND_MAX);
      r.push_back(0.001f * (float)std::rand() / RAND_MAX);
    }

    qtr::set_trees(*renderer, x, y, h, r);
    qtr::set_leaves(*renderer, x, y, h, r);
    renderer->reset_mesh(qtr::keys::mesh::leaves);
  }

  return app.exec();
}
