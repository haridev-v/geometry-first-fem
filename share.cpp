try {
    // Silence HDF5's auto error stack
    //H5::Exception::dontPrint();

    // ------------------------------------------------------------
    // Enable FPD stitching only when every HDF5 input has a sibling
    // file with the same base name and the .fpd extension.
    // ------------------------------------------------------------

    std::vector<std::string> fpd_paths;
    fpd_paths.reserve(inputs.size());

    bool merge_fpd_enabled = true;

    for (std::size_t i = 0;
         i < inputs.size();
         ++i)
    {
        const std::string fpd_path =
            sibling_with_extension(
                inputs[i].path,
                ".fpd");

        fpd_paths.push_back(fpd_path);

        if (!file_exists(fpd_path))
            merge_fpd_enabled = false;
    }

    std::vector<FpdFile> fpd_inputs;

    if (merge_fpd_enabled)
    {
        std::cout
            << "FPD files found for all mesh inputs. "
            << "FPD stitching is enabled.\n";

        fpd_inputs.reserve(inputs.size());

        for (std::size_t i = 0;
             i < fpd_paths.size();
             ++i)
        {
            std::cout
                << "Reading FPD: "
                << fpd_paths[i]
                << "\n";

            fpd_inputs.push_back(
                read_fpd_file(fpd_paths[i]));
        }
    }
    else
    {
        std::cout
            << "At least one input FPD file is missing. "
            << "Skipping the entire FPD stitching process.\n";

        for (std::size_t i = 0;
             i < fpd_paths.size();
             ++i)
        {
            if (!file_exists(fpd_paths[i]))
            {
                std::cout
                    << "  Missing: "
                    << fpd_paths[i]
                    << "\n";
            }
        }

        std::cout
            << "The available FPD files will not be read.\n";
    }

    std::vector<InputMarkerMap> input_marker_maps(
        inputs.size());

    std::vector<double> all_nodes_xyz;

{
    std::unordered_map<long long,long long> remap;
    remap.reserve(eVals.size());

    for (auto v : eVals)
        remap.emplace(v, 0LL);

    for (auto &kv : remap)
        kv.second = kv.first + global_body_offset;

    // Preserve the exact marker map used by the HDF5 stitcher.
    // This will later position RRMPDOMAIN and related FPD rows.
    auto& stored_volume_map =
        input_marker_maps
            .at(static_cast<std::size_t>(file_id))
            .volume_old_to_final;

    stored_volume_map.reserve(remap.size());

    for (const auto& kv : remap)
    {
        stored_volume_map.emplace(
            kv.first,
            kv.second);
    }

    for (auto &v : eVals)
        v = remap[v];

    global_body_offset +=
        static_cast<int>(remap.size());

    if (dbg_on())
    {
        std::cerr
            << "[volumes] " << path
            << " unique labels=" << remap.size()
            << " new global_body_offset="
            << global_body_offset
            << "\n";
    }
}

{
    std::unordered_map<long long,long long> fmap;
    fmap.reserve(fVals.size());

    for (auto v : fVals)
        fmap.emplace(v, 0LL);

    for (auto &kv : fmap)
    {
        kv.second =
            kv.first + global_facet_offset;
    }

    // Preserve:
    // source FPD/HDF5 facet marker
    // -> intermediate globally unique HDF5 marker.
    auto& stored_facet_map =
        input_marker_maps
            .at(static_cast<std::size_t>(file_id))
            .facet_old_to_intermediate;

    stored_facet_map.reserve(fmap.size());

    for (const auto& kv : fmap)
    {
        stored_facet_map.emplace(
            kv.first,
            kv.second);
    }

    for (auto &v : fVals)
        v = fmap[v];

    global_facet_offset +=
        static_cast<int>(fmap.size());
}

std::unordered_map<long long, long long>
    facet_intermediate_to_final;

if (!all_facet_vals.empty())
{
    // Gather the intermediate marker IDs that survived
    // internal-facet removal.
    std::vector<long long> uniq =
        all_facet_vals;

    std::sort(
        uniq.begin(),
        uniq.end());

    uniq.erase(
        std::unique(
            uniq.begin(),
            uniq.end()),
        uniq.end());

    // Intermediate marker -> final compact marker.
    facet_intermediate_to_final.reserve(
        uniq.size());

    long long nextId = 1;

    for (auto v : uniq)
    {
        facet_intermediate_to_final.emplace(
            v,
            nextId++);
    }

    // Apply final marker IDs to the HDF5 facet values.
    for (auto &v : all_facet_vals)
    {
        v = facet_intermediate_to_final.at(v);
    }

    if (dbg_on())
    {
        std::cerr
            << "[facets] relabeled "
            << uniq.size()
            << " unique facet IDs to 1.."
            << (nextId - 1)
            << "\n";
    }
}

const hsize_t N_unique =
    merger.count();

const hsize_t M_total =
    static_cast<hsize_t>(
        all_elem_vals.size());

const hsize_t F_total =
    static_cast<hsize_t>(
        all_facet_vals.size());

const std::string merged_fpd_path =
    sibling_with_extension(
        out_path,
        ".fpd");

FpdFile merged_fpd;

if (merge_fpd_enabled)
{
    // RRMPDOMAIN row i corresponds to HDF5 volume marker i.
    // Therefore the merged FPD must contain rows through the
    // largest marker present in merged.h5.
    std::size_t merged_domain_rows = 0;

    if (!all_elem_vals.empty())
    {
        const auto max_marker_it =
            std::max_element(
                all_elem_vals.begin(),
                all_elem_vals.end());

        if (*max_marker_it < 1)
        {
            throw std::runtime_error(
                "FPD stitching requires positive "
                "HDF5 volume-marker IDs");
        }

        merged_domain_rows =
            static_cast<std::size_t>(
                *max_marker_it);
    }

    // Build and validate the FPD before overwriting merged.h5.
    merged_fpd =
        merge_fpd_files(
            fpd_inputs,
            input_marker_maps,
            facet_intermediate_to_final,
            merged_domain_rows);
}

write_output(
    out_path,
    merger.nodes_xyz(),
    all_elem_conn4,
    all_elem_vals,
    all_facet_conn3,
    all_facet_vals,
    N_unique,
    M_total,
    F_total,
    file_infos,
    epsilon);

if (merge_fpd_enabled)
{
    write_fpd_file(
        merged_fpd_path,
        merged_fpd);

    std::cout
        << "Wrote merged FPD: "
        << merged_fpd_path
        << "\n";
}
else
{
    // The newly generated merged.h5 does not have a corresponding
    // merged FPD. Remove an old output left by an earlier run.
    if (file_exists(merged_fpd_path))
    {
        if (std::remove(
                merged_fpd_path.c_str()) != 0)
        {
            throw std::runtime_error(
                "The merged mesh was written, but the stale "
                "FPD output could not be removed: " +
                merged_fpd_path);
        }

        std::cout
            << "Removed stale FPD output: "
            << merged_fpd_path
            << "\n";
    }

    std::cout
        << "FPD stitching was skipped. "
        << "No merged.fpd was written.\n";
}

return 0;

