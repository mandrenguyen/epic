
// SPDX-License-Identifier: LGPL-3.0-or-later
// EEEMCalOptical_geo.cpp
// Full backward PbWO4 EEEMCal geometry (ported from HomogeneousCalorimeter_geo.cpp)
// but registered under epic_EEEMCalOptical and using optical-capable materials.
// This plugin BUILDS the full detector from XML parameters (placements, wrapper,
// readout, etc.) just like the standard homogeneous calorimeter. No decorator logic.

#include "DD4hep/DetFactoryHelper.h"
#include "DD4hep/Printout.h"
#include "GeometryHelpers.h"
#include <XML/Helper.h>
#include <algorithm>
#include <cmath>
#include <tuple>
#include <vector>

// Default mechanical layer thicknesses if not overridden by XML
constexpr double PCB_THICKNESS = 1.5 * dd4hep::mm;
constexpr double SENSOR_THICKNESS = 0.5 * dd4hep::mm;

using namespace dd4hep;

// ---- helpers copied from HomogeneousCalorimeter_geo.cpp ----
namespace {
  template <class XmlComp>
  Position get_xml_xyz(XmlComp& comp, dd4hep::xml::Strng_t name) {
    Position pos(0., 0., 0.);
    if (comp.hasChild(name)) {
      auto child = comp.child(name);
      pos.SetX(dd4hep::getAttrOrDefault<double>(child, _Unicode(x), 0.));
      pos.SetY(dd4hep::getAttrOrDefault<double>(child, _Unicode(y), 0.));
      pos.SetZ(dd4hep::getAttrOrDefault<double>(child, _Unicode(z), 0.));
    }
    return pos;
  }

  static Volume build_inner_support(Detector& desc, xml_comp_t handle, xml_coll_t pts_extrudedpolygon) {
    Material inner_ring_material = desc.material(handle.materialStr());

    double electron_r      = handle.attr<double>(_Unicode(electron_r));
    double proton_r        = handle.attr<double>(_Unicode(proton_r));
    double proton_x_offset = handle.attr<double>(_Unicode(proton_x_offset));
    double z_length        = handle.z_length();

    std::vector<double> pt_x, pt_y;
    for (xml_coll_t position_i(pts_extrudedpolygon, _U(position)); position_i; ++position_i) {
      xml_comp_t position_comp = position_i;
      pt_x.push_back(position_comp.x());
      pt_y.push_back(position_comp.y());
    }

    std::vector<double> sec_z  = {-z_length / 2., z_length / 2.};
    std::vector<double> sec_x  = {0., 0.};
    std::vector<double> sec_y  = {0., 0.};
    std::vector<double> zscale = {1., 1.};

    ExtrudedPolygon inner_support_envelope(pt_x, pt_y, sec_z, sec_x, sec_y, zscale);

    double straight_section_tilt = acos((electron_r - proton_r) / proton_x_offset);
    double straight_section_length = proton_x_offset + (proton_r - electron_r) * cos(straight_section_tilt);
    Position straight_section_offset{ straight_section_length / 2 + cos(straight_section_tilt) * electron_r, 0., 0. };

    double cutout_length = 2 * z_length; // any value >1 works

    Tube electron_side{0., electron_r, cutout_length / 2};
    Tube proton_side{0., proton_r, cutout_length / 2};
    Trd1 electron_proton_straight_section{ electron_r * sin(straight_section_tilt), proton_r * sin(straight_section_tilt), cutout_length / 2, straight_section_length / 2 };
    UnionSolid inner_support_hole{
      UnionSolid{ electron_side, proton_side, Position{proton_x_offset, 0., 0.} },
      electron_proton_straight_section,
      Transform3D{straight_section_offset} * RotationZ(90 * deg) * RotationX(90 * deg),
    };

    SubtractionSolid inner_support{ inner_support_envelope, inner_support_hole };

    Volume inner_support_vol{"inner_support_vol", inner_support, inner_ring_material};
    inner_support_vol.setVisAttributes(desc.visAttributes(handle.visStr()));
    return inner_support_vol;
  }

  // Build a module (crystal + SiPM + PCB + sensors) with optional wrapper
  static std::tuple<Volume, Position> build_module(Detector& desc, xml::Collection_t& plm, SensitiveDetector& sens) {
    auto mod = plm.child(_Unicode(module));
    auto mx  = mod.attr<double>(_Unicode(modulex));
    auto my  = mod.attr<double>(_Unicode(moduley));
    auto mz  = mod.attr<double>(_Unicode(modulez));
    auto mdz = mod.attr<double>(_Unicode(moduleshift));

    Box modshape(mx / 2., my / 2., mz / 2.);
    auto modMat = desc.material(mod.attr<std::string>(_Unicode(gmaterial)));
    Volume modVol("module_vol", modshape, modMat);
    modVol.setVisAttributes(desc.visAttributes(mod.attr<std::string>(_Unicode(vis))));

    // Crystal
    xml_comp_t modComp = plm.child(_Unicode(module));
    auto cry  = modComp.child(_Unicode(crystal));
    auto cryx = cry.attr<double>(_Unicode(sizex));
    auto cryy = cry.attr<double>(_Unicode(sizey));
    auto cryz = cry.attr<double>(_Unicode(sizez));

    Box crystalshape(cryx / 2., cryy / 2., cryz / 2.);
    // Force optical-capable material name if user provided a non-optical one
    std::string cryMatName = cry.attr<std::string>(_Unicode(material));
    if (cryMatName == "leadtungsten") cryMatName = "leadtungsten_optical";
    auto crystalMat = desc.material(cryMatName);
    Volume crystalVol("crystal_vol", crystalshape, crystalMat);
    // Do NOT assign SD to crystal; assign only to SiPM below.
    crystalVol.setVisAttributes(desc.visAttributes(cry.attr<std::string>(_Unicode(cryvis))));
    // Place crystal in module
    modVol.placeVolume(crystalVol, Position(0., 0., (PCB_THICKNESS + SENSOR_THICKNESS) + (cryz - mz) / 2.));

    // SiPM sensitive layer (immediately behind crystal, along +z)
    // Use same x/y as crystal, thickness = SENSOR_THICKNESS
    Box sipmshape(cryx / 2., cryy / 2., SENSOR_THICKNESS / 2.);
    auto sipmMat = desc.material("SiPM_Sensitive");
    Volume sipmVol("sipm_vol", sipmshape, sipmMat);
    if (desc.buildType() == BUILD_SIMU) {
      sipmVol.setSensitiveDetector(sens);
    }
    sipmVol.setVisAttributes(desc.visAttributes("SiPMVis")); // Optional: user can define SiPMVis in compact
    // Place SiPM just behind the crystal (+z)
    // The back face of crystal is at z = (PCB_THICKNESS + SENSOR_THICKNESS) + (cryz - mz)/2. + cryz/2
    // But in this code, crystal is centered at z = (PCB_THICKNESS + SENSOR_THICKNESS) + (cryz - mz)/2.
    // So, SiPM should be placed at: (PCB_THICKNESS) + (cryz + SENSOR_THICKNESS - mz)/2.
    // Or, equivalently, right after crystal: (PCB_THICKNESS) + (cryz + SENSOR_THICKNESS - mz)/2.
    modVol.placeVolume(sipmVol, Position(0., 0., PCB_THICKNESS + (cryz + SENSOR_THICKNESS - mz) / 2.));
    printout(INFO, "EEEMCalOptical", "Added SiPM sensitive volume behind crystal: size=%.2f x %.2f x %.2f mm", cryx / dd4hep::mm, cryy / dd4hep::mm, SENSOR_THICKNESS / dd4hep::mm);

    // Readout (PCB + sensor tiles)
    auto roc         = modComp.child(_Unicode(readout));
    auto PCBx        = roc.attr<double>(_Unicode(PCB_sizex));
    auto PCBy        = roc.attr<double>(_Unicode(PCB_sizex));
    auto PCBz        = roc.attr<double>(_Unicode(PCB_thickness));
    auto sensorx     = roc.attr<double>(_Unicode(Sensor_sizex));
    auto sensory     = roc.attr<double>(_Unicode(Sensor_sizey));
    auto sensorz     = roc.attr<double>(_Unicode(Sensor_thickness));
    auto sensorspace = roc.attr<double>(_Unicode(Sensor_space));
    auto sensorNx    = roc.attr<int>(_Unicode(Nsensor_X));
    auto sensorNy    = roc.attr<int>(_Unicode(Nsensor_Y));

    Box PCBshape(PCBx / 2., PCBy / 2., PCBz / 2.);
    auto PCBMat = desc.material(roc.attr<std::string>(_Unicode(material)));
    Volume PCBVol("PCB_vol", PCBshape, PCBMat);
    modVol.placeVolume(PCBVol, Position(0., 0., (PCBz - mz) / 2.));

    Box sensorshape(sensorx / 2., sensory / 2., sensorz / 2.);
    auto sensorMat = desc.material(roc.attr<std::string>(_Unicode(material)));
    Volume sensorVol("sensor_vol", sensorshape, sensorMat);
    auto marginx = (PCBx - sensorNx * sensorx - (sensorNx - 1) * sensorspace) / 2.;
    auto marginy = (PCBy - sensorNy * sensory - (sensorNy - 1) * sensorspace) / 2.;
    auto x0      = marginx + sensorx / 2. - PCBx / 2.;
    auto y0      = marginy + sensory / 2. - PCBy / 2.;
    for (int i = 0; i < sensorNx; i++)
      for (int j = 0; j < sensorNy; j++)
        modVol.placeVolume(sensorVol, Position(x0 + (sensorx + sensorspace) * i, y0 + (sensory + sensorspace) * j, PCBz + (sensorz - mz) / 2.));

    // Optional wrapper
    if (!plm.hasChild(_Unicode(wrapper))) {
      return std::make_tuple(modVol, Position{mx, my, mz});
    } else {
      auto wrp = plm.child(_Unicode(wrapper));
      auto wrapcfthickness = wrp.attr<double>(_Unicode(carbonfiber_thickness));
      auto wrapcflength    = wrp.attr<double>(_Unicode(carbonfiber_length));
      auto wrapVMthickness = wrp.attr<double>(_Unicode(VM2000_thickness));
      auto carbonMat       = desc.material(wrp.attr<std::string>(_Unicode(material_carbon)));
      auto wrpMat          = desc.material(wrp.attr<std::string>(_Unicode(material_wrap)));
      auto gapMat          = desc.material(wrp.attr<std::string>(_Unicode(material_gap)));

      if (wrapcfthickness < 1e-12 * mm) return std::make_tuple(modVol, Position{mx, my, mz});

      Box carbonShape(mx / 2., my / 2., wrapcflength / 2.);
      Box carbonShape_sub((mx - 2. * wrapcfthickness) / 2., (my - 2. * wrapcfthickness) / 2., wrapcflength / 2.);
      SubtractionSolid carbon_subtract(carbonShape, carbonShape_sub, Position(0., 0., 0.));

      Box gapShape(mx / 2., my / 2., (cryz - 2. * wrapcflength) / 2.);
      Box gapShape_sub((mx - 2. * wrapcfthickness) / 2., (my - 2. * wrapcfthickness) / 2., (cryz - 2. * wrapcflength) / 2.);
      SubtractionSolid gap_subtract(gapShape, gapShape_sub, Position(0., 0., 0.));

      Box wrpVM2000((mx - 2. * wrapcfthickness) / 2., (my - 2. * wrapcfthickness) / 2., (cryz + mdz) / 2.);
      Box wrpVM2000_sub((mx - 2. * wrapcfthickness - 2. * wrapVMthickness) / 2., (my - 2. * wrapcfthickness - 2. * wrapVMthickness) / 2., cryz / 2.);
      SubtractionSolid wrpVM2000_subtract(wrpVM2000, wrpVM2000_sub, Position(0., 0., -mdz / 2.));

      Volume carbonVol("carbon_vol", carbon_subtract, carbonMat);
      Volume gapVol("gap_vol", gap_subtract, gapMat);
      Volume wrpVol("wrapper_vol", wrpVM2000_subtract, wrpMat);

      modVol.placeVolume(carbonVol, Position(0., 0., PCBz + SENSOR_THICKNESS + (wrapcflength - mz) / 2.));
      modVol.placeVolume(carbonVol, Position(0., 0., PCBz + SENSOR_THICKNESS + cryz - (wrapcflength + mz) / 2.));
      modVol.placeVolume(gapVol,    Position(0., 0., PCBz + SENSOR_THICKNESS + (cryz - mz) / 2.));
      modVol.placeVolume(wrpVol,    Position(0., 0., PCBz + SENSOR_THICKNESS + (cryz + mdz - mz) / 2.));

      carbonVol.setVisAttributes(desc.visAttributes(wrp.attr<std::string>(_Unicode(vis_carbon))));
      gapVol.setVisAttributes(desc.visAttributes(wrp.attr<std::string>(_Unicode(vis_gap))));
      wrpVol.setVisAttributes(desc.visAttributes(wrp.attr<std::string>(_Unicode(vis_wrap))));

      return std::make_tuple(modVol, Position{mx, my, mz});
    }
  }

  static std::tuple<int, std::pair<int,int>> add_12surface_disk(Detector& desc, Assembly& env, xml::Collection_t& plm, SensitiveDetector& sens, int sid) {
    auto [modVol, modSize]        = build_module(desc, plm, sens);
    int sector_id                 = dd4hep::getAttrOrDefault<int>(plm, _Unicode(sector), sid);
    double rmax                   = plm.attr<double>(_Unicode(rmax));
    double r12min                 = plm.attr<double>(_Unicode(r12min));
    double r12max                 = plm.attr<double>(_Unicode(r12max));
    double structure_frame_length = plm.attr<double>(_Unicode(outerringlength));
    double envelope_length        = plm.attr<double>(_Unicode(envelope_length));
    double Prot                   = plm.attr<double>(_Unicode(protate));
    double Nrot                   = plm.attr<double>(_Unicode(nrotate));
    double Oring_shift            = plm.attr<double>(_Unicode(outerringshift));
    double Innera                 = plm.attr<double>(_Unicode(inneradiusa));
    double Innerb                 = plm.attr<double>(_Unicode(inneradiusb));
    double phimin                 = dd4hep::getAttrOrDefault<double>(plm, _Unicode(phimin), 0.);
    double phimax                 = dd4hep::getAttrOrDefault<double>(plm, _Unicode(phimax), 2.*M_PI);
    xml_coll_t pts_extrudedpolygon(plm, _Unicode(points_extrudedpolygon));

    double half_modx = modSize.x()*0.5, half_mody = modSize.y()*0.5;

    // Outer supporting frame
    Material outer_ring_material = desc.material(getAttrOrDefault<std::string>(plm, _U(material), "StainlessSteelSAE304"));
    PolyhedraRegular solid_ring12(12, r12min, r12max, structure_frame_length);
    Volume ring12_vol("ring12", solid_ring12, outer_ring_material);
    Transform3D tr_global_Oring = RotationZYX(Prot, 0., 0.) * Translation3D(0., 0., Oring_shift);
    ring12_vol.setVisAttributes(desc.visAttributes(plm.attr<std::string>(_Unicode(vis_struc))));

    // Envelope for modules
    bool has_envelope = dd4hep::getAttrOrDefault<bool>(plm, _Unicode(envelope), false);
    PolyhedraRegular solid_world(12, 0., r12min, envelope_length);
    EllipticalTube solid_sub(Innera, Innerb, envelope_length / 2.);
    Transform3D subtract_pos = RotationZYX(Nrot, 0., 0.) * Translation3D(1 * cm, 0., 0.);
    SubtractionSolid calo_subtract(solid_world, solid_sub, subtract_pos);
    Volume env_vol(std::string(env.name()) + "_envelope", calo_subtract, desc.material("Air"));
    Transform3D tr_global = RotationZYX(Prot, 0., 0.) * Translation3D(0., 0., 0.);
    env_vol.setVisAttributes(desc.visAttributes(plm.attr<std::string>(_Unicode(vis_steel_gap))));

    if (has_envelope) {
      env.placeVolume(env_vol, tr_global);
      env.placeVolume(ring12_vol, tr_global_Oring);

      xml_comp_t collar_comp   = plm.child(_Unicode(inner_support));
      Volume inner_support_vol = build_inner_support(desc, collar_comp, pts_extrudedpolygon);
      env_vol.placeVolume(inner_support_vol, Transform3D{RotationZ{Nrot}} * Translation3D(collar_comp.x_offset(0.), collar_comp.y_offset(0.), collar_comp.z_offset(0.)));
    }

    // Module placement grid
    xml_comp_t placement = plm.child(_Unicode(placement));
    auto points = epic::geo::fillRectangles({placement.x_offset(0.), placement.y_offset(0.)}, modSize.x(), modSize.y(), 0., (rmax / std::cos(Prot)), phimin, phimax);

    std::pair<double,double> c1(0., 0.);
    auto polyVertex = epic::geo::getPolygonVertices(c1, (rmax / std::cos(Prot)), M_PI / 12., 12);
    std::vector<epic::geo::Point> out_vertices, in_vertices;
    for (auto p : polyVertex) out_vertices.push_back({p.first, p.second});
    for (xml_coll_t position_i(pts_extrudedpolygon, _U(position)); position_i; ++position_i) {
      xml_comp_t position_comp = position_i; in_vertices.push_back({position_comp.x(), position_comp.y()});
    }

    double minX = 0., maxX = 0., minY = 0., maxY = 0.;
    for (auto& square : points) {
      epic::geo::Point box[4] = {{square.x() + half_modx, square.y() + half_mody},
                                 {square.x() - half_modx, square.y() + half_mody},
                                 {square.x() - half_modx, square.y() - half_mody},
                                 {square.x() + half_modx, square.y() - half_mody}};
      if (epic::geo::isBoxTotalInsidePolygon(box, out_vertices)) {
        if (square.x() < minX) minX = square.x();
        if (square.y() < minY) minY = square.y();
        if (square.x() > maxX) maxX = square.x();
        if (square.y() > maxY) maxY = square.y();
      }
    }

    int total_count = 0;
    int N_row      = std::round((maxY - minY) / modSize.y());
    int N_column   = std::round((maxX - minX) / modSize.x());
    auto rowcolumn = std::make_pair(N_row, N_column);

    for (auto& square : points) {
      epic::geo::Point box[4] = {{square.x() + half_modx, square.y() + half_mody},
                                 {square.x() - half_modx, square.y() + half_mody},
                                 {square.x() - half_modx, square.y() - half_mody},
                                 {square.x() + half_modx, square.y() - half_mody}};

      if (epic::geo::isBoxTotalInsidePolygon(box, out_vertices) && !epic::geo::isBoxTotalInsidePolygon(box, in_vertices)) {
        int column = std::round((square.x() - minX) / modSize.x());
        int row    = std::round((maxY - square.y()) / modSize.y());
        Transform3D tr_local = RotationZYX(Nrot, 0.0, 0.0) * Translation3D(square.x(), square.y(), 0.);
        auto modPV = (has_envelope ? env_vol.placeVolume(modVol, tr_local) : env.placeVolume(modVol, tr_global * tr_local));
        modPV.addPhysVolID("sector", sector_id).addPhysVolID("row", row).addPhysVolID("column", column);
        total_count++;
      }
    }

    printout(DEBUG, "EEEMCalOptical", "Placed %d modules in sector %d", total_count, sector_id);
    return {sector_id, rowcolumn};
  }
}

static Ref_t create_detector(Detector& desc, xml::Handle_t handle, SensitiveDetector sens) {
  xml::DetElement detElem = handle;
  std::string detName     = detElem.nameStr();
  int detID               = detElem.id();
  DetElement det(detName, detID);

  // Use generic SD to ensure we create a hits collection; optical filters will route photons
  if (desc.buildType() == BUILD_SIMU) sens.setType("tracker");

  // top assembly container for this detector
  Assembly assembly(detName);

  // build all disks (12-surface layout) exactly like the standard homogeneous calorimeter
  xml::Component plm = detElem.child(_Unicode(placements));
  std::map<int, std::pair<int, int>> sectorModuleRowsColumns;
  auto addRowColumnNumbers = [&sectorModuleRowsColumns](int sector, std::pair<int, int> rowcolumn) {
    auto it = sectorModuleRowsColumns.find(sector);
    if (it != sectorModuleRowsColumns.end()) it->second = rowcolumn; else sectorModuleRowsColumns[sector] = rowcolumn;
  };

  int sector_id = 1;
  for (xml::Collection_t disk_12surface(plm, _Unicode(disk_12surface)); disk_12surface; ++disk_12surface) {
    auto [sector, rowcolumn] = add_12surface_disk(desc, assembly, disk_12surface, sens, sector_id++);
    addRowColumnNumbers(sector, rowcolumn);
  }

  for (auto [sector, rowcolumn] : sectorModuleRowsColumns) {
    desc.add(Constant(Form((detName + "_NModules_Sector%d").c_str(), sector), std::to_string((rowcolumn.first)), std::to_string((rowcolumn.second))));
  }

  // place under world using optional position/rotation from XML
  auto pos         = get_xml_xyz(detElem, _Unicode(position));
  auto rot         = get_xml_xyz(detElem, _Unicode(rotation));
  Volume motherVol = desc.pickMotherVolume(det);
  Transform3D tr = Translation3D(pos.x(), pos.y(), pos.z()) * RotationZYX(rot.z(), rot.y(), rot.x());
  PlacedVolume envPV = motherVol.placeVolume(assembly, tr);
  envPV.addPhysVolID("system", detID);
  det.setPlacement(envPV);

  // --- Diagnostic placement check ---
  auto world = desc.world();
  if (world.isValid()) {
    printout(INFO, "EEEMCalOptical",
             "World volume name: %s", world.name());
    auto daughters = world.children();
    printout(INFO, "EEEMCalOptical",
             "World has %zu registered DetElements before adding this one.", daughters.size());
  } else {
    printout(ERROR, "EEEMCalOptical", "World volume not valid!");
  }

  printout(INFO, "EEEMCalOptical",
           "Added %s (ID %d) to detector description. Now world has %zu subdetectors.",
           detName.c_str(), detID, desc.detectors().size());

  printout(INFO, "EEEMCalOptical", "Full EEEMCalOptical geometry constructed (ID %d)", detID);
  return det;
}

DECLARE_DETELEMENT(epic_EEEMCalOptical, create_detector)