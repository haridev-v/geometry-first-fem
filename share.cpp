// ------------------------------------------------------------
// Enable FPD stitching only when every HDF5 input has a sibling
// FPD with the same base filename.
// ------------------------------------------------------------

std::vector<std::string> fpd_paths;
fpd_paths.reserve(inputs.size());

bool merge_fpd_enabled = true;

std::cerr
    << "\n========== FPD detection ==========\n";

for (std::size_t i = 0;
     i < inputs.size();
     ++i)
{
    const std::string fpd_path =
        sibling_with_extension(
            inputs[i].path,
            ".fpd");

    const bool found =
        file_exists(fpd_path);

    fpd_paths.push_back(fpd_path);

    std::cerr
        << "HDF5 input [" << i << "]: "
        << inputs[i].path
        << "\n";

    std::cerr
        << "Expected FPD [" << i << "]: "
        << fpd_path
        << "\n";

    std::cerr
        << "Status: "
        << (found ? "FOUND" : "MISSING")
        << "\n\n";

    if (!found)
        merge_fpd_enabled = false;
}

std::vector<FpdFile> fpd_inputs;

if (merge_fpd_enabled)
{
    std::cerr
        << "Every input FPD was found. "
        << "FPD stitching is ENABLED.\n";

    fpd_inputs.reserve(inputs.size());

    for (std::size_t i = 0;
         i < fpd_paths.size();
         ++i)
    {
        std::cerr
            << "Reading FPD: "
            << fpd_paths[i]
            << "\n";

        fpd_inputs.push_back(
            read_fpd_file(fpd_paths[i]));
    }

    std::cerr
        << "Successfully parsed "
        << fpd_inputs.size()
        << " FPD files.\n";
}
else
{
    std::cerr
        << "At least one expected FPD is missing.\n"
        << "The entire FPD stitching process is DISABLED.\n"
        << "Only the mesh will be stitched.\n";
}

std::cerr
    << "===================================\n\n"
    << std::flush;

std::vector<InputMarkerMap> input_marker_maps(
    inputs.size());
