/**********************************************************************
 * This file is used for testing random stuff without running the
 * whole of SatDump, which comes in handy for debugging individual
 * elements before putting them all together in modules...
 *
 * If you are an user, ignore this file which will not be built by
 * default, and if you're a developper in need of doing stuff here...
 * Go ahead!
 *
 * Don't judge the code you might see in there! :)
 **********************************************************************/

#include "logger.h"
#include "common/image/image.h"

#include "products/image_products.h"
#include "common/tracking/tracking.h"
#include "common/geodetic/euler_raytrace.h"
#include "common/geodetic/vincentys_calculations.h"
#include "common/map/map_drawer.h"
#include "resources.h"

#include <iostream>
#include "common/projection/warp/warp.h"
#include "core/config.h"

#include "common/projection/gcp_compute/gcp_compute.h"

#include "common/utils.h"

#include "common/projection/projs/equirectangular.h"

#include "core/config.h"
#include "products/dataset.h"
#include "products/processor/processor.h"

#include "common/projection/sat_proj/sat_proj.h"

#include "common/projection/projs/stereo.h"

#include "common/projection/projs/tpers.h"

#include "common/projection/reprojector.h"

int main(int argc, char *argv[])
{
    initLogger();

    std::string user_path = std::string(getenv("HOME")) + "/.config/satdump";
    satdump::config::loadConfig("satdump_cfg.json", user_path);

    // satdump::ImageProducts img_pro;
    // img_pro.load(argv[1]);

    satdump::ImageCompositeCfg rgb_cfg;
    rgb_cfg.equation = "ch3,ch2,ch1"; //"(ch3 * 0.4 + ch2 * 0.6) * 2.2 - 0.15, ch2 * 2.2 - 0.15, ch1 * 2.2 - 0.15";
    rgb_cfg.equalize = true;
    rgb_cfg.white_balance = true;

    satdump::reprojection::ReprojectionOperation op;
    op.source_prj_info = nlohmann::json::parse("{\"type\":\"geos\"}"); // img_pro.get_proj_cfg();
    op.img.load_png(argv[1]);                                          //= img_pro.images[0].image; // satdump::make_composite_from_product(img_pro, rgb_cfg);
    // op.img_tle = img_pro.get_tle();
    // op.img_tim = img_pro.get_timestamps();

    op.img.equalize(); // TMP

    op.use_draw_algorithm = false;
    op.output_width = 2048 * 4;
    op.output_height = 2048 * 4 / 2; // / 2;

    op.target_prj_info = nlohmann::json::parse("{\"type\":\"equirectangular\",\"tl_lon\":-180,\"tl_lat\":90,\"br_lon\":180,\"br_lat\":-90}");
    // op.target_prj_info = nlohmann::json::parse("{\"type\":\"stereo\",\"center_lon\":20.8,\"center_lat\":48,\"scale\":2}");
    //  op.target_prj_info = nlohmann::json::parse("{\"type\":\"tpers\",\"lon\":45.0,\"lat\":0.0,\"alt\":30000,\"ang\":0,\"azi\":0}");

    satdump::reprojection::ProjectionResult ret = satdump::reprojection::reproject(op);

    logger->info("Drawing map");
    if (op.target_prj_info["type"] == "equirectangular")
    {
        geodetic::projection::EquirectangularProjection projector;
        projector.init(ret.img.width(), ret.img.height(),
                       op.target_prj_info["tl_lon"].get<float>(), op.target_prj_info["tl_lat"].get<float>(),
                       op.target_prj_info["br_lon"].get<float>(), op.target_prj_info["br_lat"].get<float>());

        unsigned short color[3] = {0, 65535, 0};
        map::drawProjectedMapShapefile({resources::getResourcePath("maps/ne_10m_admin_0_countries.shp")},
                                       ret.img,
                                       color,
                                       [&projector](float lat, float lon, int map_height2, int map_width2) -> std::pair<int, int>
                                       {
                                           int x, y;
                                           projector.forward(lon, lat, x, y);
                                           return {x, y};
                                       });
        unsigned short color2[3] = {65535, 0, 0};
        map::drawProjectedCapitalsGeoJson(
            {resources::getResourcePath("maps/ne_10m_populated_places_simple.json")},
            ret.img,
            color2,
            [&projector](float lat, float lon, int map_height2, int map_width2) -> std::pair<int, int>
            {
                int x, y;
                projector.forward(lon, lat, x, y);
                return {x, y};
            },
            0.5);
    }
    else if (op.target_prj_info["type"] == "stereo")
    {
        geodetic::projection::StereoProjection stereo_proj;
        stereo_proj.init(op.target_prj_info["center_lat"].get<float>(), op.target_prj_info["center_lon"].get<float>());
        float stereo_scale = op.target_prj_info["scale"].get<float>();

        unsigned short color[3] = {0, 65535, 0};
        map::drawProjectedMapShapefile({resources::getResourcePath("maps/ne_10m_admin_0_countries.shp")},
                                       ret.img,
                                       color,
                                       [&stereo_proj, stereo_scale](float lat, float lon, int map_height, int map_width) -> std::pair<int, int>
                                       {
                                           double x = 0;
                                           double y = 0;
                                           stereo_proj.forward(lon, lat, x, y);
                                           x *= map_width / stereo_scale;
                                           y *= map_height / stereo_scale;
                                           return {x + (map_width / 2), map_height - (y + (map_height / 2))};
                                       });
        unsigned short color2[3] = {65535, 0, 0};
        map::drawProjectedCapitalsGeoJson(
            {resources::getResourcePath("maps/ne_10m_populated_places_simple.json")},
            ret.img,
            color2,
            [&stereo_proj, stereo_scale](float lat, float lon, int map_height, int map_width) -> std::pair<int, int>
            {
                double x = 0;
                double y = 0;
                stereo_proj.forward(lon, lat, x, y);
                x *= map_width / stereo_scale;
                y *= map_height / stereo_scale;
                return {x + (map_width / 2), map_height - (y + (map_height / 2))};
            },
            0.5);
    }
    else if (op.target_prj_info["type"] == "tpers")
    {
        geodetic::projection::TPERSProjection tpers_proj;
        tpers_proj.init(op.target_prj_info["alt"].get<float>() * 1000,
                        op.target_prj_info["lon"].get<float>(),
                        op.target_prj_info["lat"].get<float>(),
                        op.target_prj_info["ang"].get<float>(),
                        op.target_prj_info["azi"].get<float>());

        unsigned short color[3] = {0, 65535, 0};
        map::drawProjectedMapShapefile({resources::getResourcePath("maps/ne_10m_admin_0_countries.shp")},
                                       ret.img,
                                       color,
                                       [&tpers_proj](float lat, float lon, int map_height, int map_width) -> std::pair<int, int>
                                       {
                                           double x, y;
                                           tpers_proj.forward(lon, lat, x, y);
                                           x *= map_width / 2;
                                           y *= map_height / 2;
                                           return {x + (map_width / 2), map_height - (y + (map_height / 2))};
                                       });
        unsigned short color2[3] = {65535, 0, 0};
        /*map::drawProjectedCapitalsGeoJson(
            {resources::getResourcePath("maps/ne_10m_populated_places_simple.json")},
            ret.img,
            color2,
            [&tpers_proj](float lat, float lon, int map_height, int map_width) -> std::pair<int, int>
            {
                double x, y;
                tpers_proj.forward(lon, lat, x, y);
                x *= map_width / 2;
                y *= map_height / 2;
                return {x + (map_width / 2), map_height - (y + (map_height / 2))};
            },
            0.5);*/
    }

    logger->info("Saving...");
    ret.img.save_png("test_newproj.png");

#if 0
#if 0
    image::Image<uint16_t> mersi_ch1, mersi_ch2;
    mersi_ch1.load_png("/home/alan/Documents/SatDump_ReWork/build/fy3_mersi_offset/MERSI-1/MERSI1-1.png");
    mersi_ch2.load_png("/home/alan/Documents/SatDump_ReWork/build/fy3_mersi_offset/MERSI-1/MERSI1-20.png");

    mersi_ch2.resize(mersi_ch2.width() * 4, mersi_ch2.height() * 4);

    mersi_ch1.equalize();
    mersi_ch2.equalize();

    image::Image<uint16_t> mersi_rgb(mersi_ch1.width(), mersi_ch1.height(), 3);

    mersi_rgb.draw_image(0, mersi_ch1, 0);
    mersi_rgb.draw_image(1, mersi_ch1, 0);
    mersi_rgb.draw_image(2, mersi_ch2, -24);

    mersi_rgb.save_png("mersi_test.png");

    return 0; // TMP
#endif

    initLogger();

    std::string user_path = std::string(getenv("HOME")) + "/.config/satdump";
    satdump::config::loadConfig("satdump_cfg.json", user_path);

    satdump::ImageProducts img_pro;
    img_pro.load(argv[1]);

    nlohmann::json metop_avhrr_cfg;
    // metop_avhrr_cfg.tles = img_pro.get_tle();
    // metop_avhrr_cfg.timestamps = img_pro.get_timestamps(0);
    metop_avhrr_cfg["type"] = "normal_single_line";
    metop_avhrr_cfg["img_width"] = 2048;
    metop_avhrr_cfg["timestamp_offset"] = -0.3;
    metop_avhrr_cfg["scan_angle"] = 110.6;
    metop_avhrr_cfg["roll_offset"] = -0.13;
    metop_avhrr_cfg["gcp_spacing_x"] = 100;
    metop_avhrr_cfg["gcp_spacing_y"] = 100;

    nlohmann::json metop_iasi_img_cfg;
    // metop_mhs_cfg.tles = img_pro.get_tle();
    // metop_mhs_cfg.timestamps = img_pro.get_timestamps(0);
    // metop_mhs_cfg.image_width = 90;
    // metop_mhs_cfg.timestamp_offset = -2;
    // metop_mhs_cfg.scan_angle = 100;
    // metop_mhs_cfg.roll_offset = -1.1;
    // metop_mhs_cfg.gcp_spacing_x = 5;
    // metop_mhs_cfg.gcp_spacing_y = 5;
    metop_iasi_img_cfg["type"] = "normal_single_line";
    metop_iasi_img_cfg["img_width"] = 2048;
    metop_iasi_img_cfg["timestamp_offset"] = -0.3;
    metop_iasi_img_cfg["scan_angle"] = 100;
    metop_iasi_img_cfg["roll_offset"] = -1.7;
    metop_iasi_img_cfg["gcp_spacing_x"] = 100;
    metop_iasi_img_cfg["gcp_spacing_y"] = 100;

    // logger->trace("\n" + img_pro.contents.dump(4));

    // img_pro.images[3].image.equalize();

    // std::shared_ptr<satdump::SatelliteProjection> satellite_proj = satdump::get_sat_proj(img_pro.get_proj_cfg(), img_pro.get_tle(), img_pro.get_timestamps(0));
    // geodetic::projection::EquirectangularProjection projector_target;
    // projector_target.init(2048 * 4, 1024 * 4, -180, 90, 180, -90);

#if 0
    image::Image<uint16_t> final_image(2048 * 4, 1024 * 4, 3);

    int radius = 0;

    for (int img_y = 0; img_y < img_pro.images[0].image.height(); img_y++)
    {
        for (int img_x = 0; img_x < img_pro.images[0].image.width(); img_x++)
        {
            geodetic::geodetic_coords_t position;
            geodetic::geodetic_coords_t position2, position3;
            if (!satellite_proj->get_position(img_x, img_y, position))
            {
                int target_x, target_y;
                projector_target.forward(position.lon, position.lat, target_x, target_y);

                if (!satellite_proj->get_position(img_x + 1, img_y, position2) && !satellite_proj->get_position(img_x, img_y, position3))
                {
                    int target_x2, target_y2;
                    projector_target.forward(position2.lon, position2.lat, target_x2, target_y2);

                    int target_x3, target_y3;
                    projector_target.forward(position3.lon, position3.lat, target_x3, target_y3);

                    int new_radius1 = abs(target_x2 - target_x);
                    int new_radius2 = abs(target_y2 - target_y);

                    int new_radius = std::max(new_radius1, new_radius2);

                    int max = img_pro.images[0].image.width() / 16;

                    if (new_radius < max)
                        radius = new_radius;
                    else if (new_radius1 < max)
                        radius = new_radius1;
                    else if (new_radius2 < max)
                        radius = new_radius2;
                }

                uint16_t color[3];
                color[0] = img_pro.images[3].image.channel(0)[img_y * img_pro.images[0].image.width() + img_x];
                color[1] = img_pro.images[3].image.channel(0)[img_y * img_pro.images[0].image.width() + img_x];
                color[2] = img_pro.images[3].image.channel(0)[img_y * img_pro.images[0].image.width() + img_x];

                final_image.draw_circle(target_x, target_y, radius, color, true);
            }
        }

        logger->info("{:d} / {:d}", img_y, img_pro.images[0].image.height());
    }

#endif

    nlohmann::ordered_json fdsfsdf = img_pro.get_proj_cfg();
    fdsfsdf["yaw_offset"] = 2;
    fdsfsdf["roll_offset"] = -1.5;
    fdsfsdf["scan_angle"] = 110;
    fdsfsdf["timestamp_offset"] = 0;

    logger->trace("\n" + img_pro.contents.dump(4));

    std::vector<satdump::projection::GCP> gcps = satdump::gcp_compute::compute_gcps(loadJsonFile(resources::getResourcePath("projections_settings/metop_abc_avhrr.json")), img_pro.get_tle(), img_pro.get_timestamps(0));

    satdump::ImageCompositeCfg rgb_cfg;
    rgb_cfg.equation = "ch3,ch2,ch1"; //"(ch3 * 0.4 + ch2 * 0.6) * 2.2 - 0.15, ch2 * 2.2 - 0.15, ch1 * 2.2 - 0.15";
    rgb_cfg.equalize = true;
    rgb_cfg.white_balance = true;

    // img_pro.images[0].image.equalize();
    // img_pro.images[0].image.to_rgb();

    satdump::warp::WarpOperation operation;
    operation.ground_control_points = gcps;
    operation.input_image = satdump::make_composite_from_product(img_pro, rgb_cfg);
    operation.output_width = 2048 * 10;
    operation.output_height = 1024 * 10;

    satdump::warp::ImageWarper warper;
    warper.op = operation;
    warper.update();

    satdump::warp::WarpResult result = warper.warp();

#if 0
    logger->info("Reproject to stereo...");

    geodetic::projection::EquirectangularProjection projector_equ;
    projector_equ.init(result.output_image.width(), result.output_image.height(), result.top_left.lon, result.top_left.lat, result.bottom_right.lon, result.bottom_right.lat);

    double stereo_scale = 6400;

    geodetic::projection::StereoProjection projector_stereo;
    projector_stereo.init(48.7, 1.8);

    image::Image<uint16_t> stereo_image(2048 * 2, 2048 * 2, 3);

    for (int x = 0; x < 2048 * 2; x++)
    {
        for (int y = 0; y < 2048 * 2; y++)
        {
            double lat, lon;

            double y2 = (double(stereo_image.height() - y) - double(stereo_image.height() / 2)) * (1.0 / stereo_scale);
            double x2 = (x - double(stereo_image.width() / 2)) * (1.0 / stereo_scale);

            int y3, x3;

            if (!projector_stereo.inverse(x2, y2, lon, lat))
            {
                projector_equ.forward(lon, lat, x3, y3);

                if (x3 != -1 && y3 != -1)
                {
                    for (int i = 0; i < 3; i++)
                        stereo_image.channel(i)[y * stereo_image.width() + x] = result.output_image.channel(i)[y3 * result.output_image.width() + x3];
                }
            }
        }
    }

    logger->info("Drawing map...");

    unsigned short color[3] = {0, 65535, 0};
    map::drawProjectedMapShapefile({resources::getResourcePath("maps/ne_10m_admin_0_countries.shp")},
                                   stereo_image,
                                   color,
                                   [&projector_stereo, stereo_scale](float lat, float lon, int map_height, int map_width) -> std::pair<int, int>
                                   {
                                       double x, y;
                                       if (projector_stereo.forward(lon, lat, x, y))
                                           return {-1, -1};
                                       x *= stereo_scale;
                                       y *= stereo_scale;
                                       return {x + (map_width / 2), map_height - (y + (map_height / 2))};
                                   });

    // img_map.crop(p_x_min, p_y_min, p_x_max, p_y_max);
    logger->info("Saving...");
#endif

    // result.output_image.save_png("test.png");

#if 1
    geodetic::projection::EquirectangularProjection projector;
    projector.init(result.output_image.width(), result.output_image.height(), result.top_left.lon, result.top_left.lat, result.bottom_right.lon, result.bottom_right.lat);

    unsigned short color[3] = {0, 65535, 0};
    map::drawProjectedMapShapefile({resources::getResourcePath("maps/ne_10m_admin_0_countries.shp")},
                                   result.output_image,
                                   color,
                                   [&projector](float lat, float lon, int map_height2, int map_width2) -> std::pair<int, int>
                                   {
                                       int x, y;
                                       projector.forward(lon, lat, x, y);
                                       return {x, y};
                                   });

    // img_map.crop(p_x_min, p_y_min, p_x_max, p_y_max);
    logger->info("Saving...");

    result.output_image.save_png("test.png");
#endif

#if 0
    unsigned short color[3] = {0, 65535, 0};
    map::drawProjectedMapShapefile({resources::getResourcePath("maps/ne_10m_admin_0_countries.shp")},
                                   final_image,
                                   color,
                                   [&projector_target](float lat, float lon, int map_height2, int map_width2) -> std::pair<int, int>
                                   {
                                       int x, y;
                                       projector_target.forward(lon, lat, x, y);
                                       return {x, y};
                                   });

    logger->info("Saving...");
    final_image.save_png("test.png");
#endif
#endif
}
