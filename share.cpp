// -------- FPD merge support --------

struct InputMarkerMap
{
    // Original volume marker in one input HDF5 file
    // -> volume marker written to merged.h5.
    std::unordered_map<long long, long long> volume_old_to_final;

    // Original facet marker in one input HDF5 file
    // -> globally unique marker used before internal facets are removed.
    std::unordered_map<long long, long long> facet_old_to_intermediate;
};

struct FpdBlock
{
    std::string name;
    int type_code = 0;
    std::size_t rows = 0;
    std::size_t cols = 0;

    std::vector<long long> integers;
    std::vector<double> reals;

    // Used for string or otherwise unrecognised block types.
    // Such blocks are preserved only when identical in every input FPD.
    std::vector<std::string> raw_lines;
};

struct FpdFile
{
    std::vector<FpdBlock> blocks;
    std::unordered_map<std::string, std::size_t> index;

    void rebuild_index()
    {
        index.clear();

        for (std::size_t i = 0; i < blocks.size(); ++i)
        {
            if (!index.emplace(blocks[i].name, i).second)
            {
                throw std::runtime_error(
                    "Duplicate FPD block: " + blocks[i].name);
            }
        }
    }

    bool has(const std::string& name) const
    {
        return index.find(name) != index.end();
    }

    const FpdBlock& at(const std::string& name) const
    {
        const auto it = index.find(name);

        if (it == index.end())
        {
            throw std::runtime_error(
                "Missing FPD block: " + name);
        }

        return blocks[it->second];
    }

    void set(const FpdBlock& block)
    {
        const auto it = index.find(block.name);

        if (it == index.end())
        {
            index.emplace(block.name, blocks.size());
            blocks.push_back(block);
        }
        else
        {
            blocks[it->second] = block;
        }
    }
};

static std::string trim_copy(const std::string& text)
{
    std::size_t first = 0;

    while (first < text.size() &&
           std::isspace(
               static_cast<unsigned char>(text[first])))
    {
        ++first;
    }

    std::size_t last = text.size();

    while (last > first &&
           std::isspace(
               static_cast<unsigned char>(text[last - 1])))
    {
        --last;
    }

    return text.substr(first, last - first);
}

static bool parse_fpd_header_line(
    const std::string& line,
    FpdBlock& block)
{
    std::istringstream in(line);
    std::string extra;

    if (!(in >> block.name
             >> block.type_code
             >> block.rows
             >> block.cols))
    {
        return false;
    }

    if (block.name.empty() || block.type_code <= 0)
        return false;

    // A normal block header contains exactly four fields.
    if (in >> extra)
        return false;

    return true;
}

static std::string sibling_with_extension(
    const std::string& path,
    const std::string& extension)
{
    const std::size_t slash =
        path.find_last_of("/\\");

    const std::size_t dot =
        path.find_last_of('.');

    if (dot == std::string::npos ||
        (slash != std::string::npos && dot < slash))
    {
        return path + extension;
    }

    return path.substr(0, dot) + extension;
}

static bool file_exists(const std::string& path)
{
    std::ifstream in(path.c_str());
    return in.good();
}

static FpdFile read_fpd_file(const std::string& path)
{
    std::ifstream in(path.c_str());

    if (!in)
    {
        throw std::runtime_error(
            "Unable to open FPD file: " + path);
    }

    std::string line;
    std::size_t block_count = 0;
    bool count_read = false;

    // First non-empty line is the number of blocks.
    while (std::getline(in, line))
    {
        const std::string trimmed = trim_copy(line);

        if (trimmed.empty())
            continue;

        std::istringstream count_stream(trimmed);
        std::string extra;

        if (!(count_stream >> block_count) ||
            (count_stream >> extra))
        {
            throw std::runtime_error(
                "Invalid FPD block count in: " + path);
        }

        count_read = true;
        break;
    }

    if (!count_read)
    {
        throw std::runtime_error(
            "Unable to read FPD block count from: " + path);
    }

    FpdFile result;
    result.blocks.reserve(block_count);

    std::string pending_header;

    for (std::size_t block_index = 0;
         block_index < block_count;
         ++block_index)
    {
        std::string header_line;

        if (!pending_header.empty())
        {
            header_line = pending_header;
            pending_header.clear();
        }
        else
        {
            while (std::getline(in, line))
            {
                const std::string trimmed = trim_copy(line);

                if (!trimmed.empty())
                {
                    header_line = trimmed;
                    break;
                }
            }
        }

        FpdBlock block;

        if (header_line.empty() ||
            !parse_fpd_header_line(header_line, block))
        {
            throw std::runtime_error(
                "Unable to read FPD block header " +
                std::to_string(block_index + 1) +
                " from: " + path);
        }

        const std::size_t value_count =
            block.rows * block.cols;

        if (block.type_code == 1 ||
            block.type_code == 2)
        {
            // Type 1 = integer.
            // Type 2 = real.
            std::size_t values_read = 0;

            if (block.type_code == 1)
                block.integers.reserve(value_count);
            else
                block.reals.reserve(value_count);

            while (values_read < value_count &&
                   std::getline(in, line))
            {
                const std::string trimmed =
                    trim_copy(line);

                if (trimmed.empty())
                    continue;

                std::istringstream values(trimmed);

                while (values_read < value_count)
                {
                    if (block.type_code == 1)
                    {
                        long long value = 0;

                        if (!(values >> value))
                            break;

                        block.integers.push_back(value);
                    }
                    else
                    {
                        double value = 0.0;

                        if (!(values >> value))
                            break;

                        block.reals.push_back(value);
                    }

                    ++values_read;
                }

                std::string extra;

                if (values_read == value_count &&
                    (values >> extra))
                {
                    throw std::runtime_error(
                        "Too many values in FPD block " +
                        block.name + " in: " + path);
                }
            }

            if (values_read != value_count)
            {
                throw std::runtime_error(
                    "Not enough values in FPD block " +
                    block.name + " in: " + path);
            }
        }
        else
        {
            // The exact string datatype code has not yet been confirmed.
            // Preserve such blocks verbatim until the next block header.
            while (std::getline(in, line))
            {
                FpdBlock possible_header;
                const std::string trimmed =
                    trim_copy(line);

                if (block_index + 1 < block_count &&
                    !trimmed.empty() &&
                    parse_fpd_header_line(
                        trimmed,
                        possible_header))
                {
                    pending_header = trimmed;
                    break;
                }

                block.raw_lines.push_back(line);
            }

            while (!block.raw_lines.empty() &&
                   trim_copy(
                       block.raw_lines.back()).empty())
            {
                block.raw_lines.pop_back();
            }
        }

        result.blocks.push_back(block);
    }

    result.rebuild_index();
    return result;
}

static void write_fpd_file(
    const std::string& path,
    const FpdFile& fpd)
{
    std::ofstream out(
        path.c_str(),
        std::ios::trunc);

    if (!out)
    {
        throw std::runtime_error(
            "Unable to create FPD file: " + path);
    }

    out << fpd.blocks.size() << "\n\n";
    out << std::setprecision(17);

    for (const auto& block : fpd.blocks)
    {
        out << block.name << " "
            << block.type_code << " "
            << block.rows << " "
            << block.cols << "\n";

        if (block.type_code == 1)
        {
            if (block.integers.size() !=
                block.rows * block.cols)
            {
                throw std::runtime_error(
                    "Invalid integer block size: " +
                    block.name);
            }

            for (std::size_t row = 0;
                 row < block.rows;
                 ++row)
            {
                for (std::size_t col = 0;
                     col < block.cols;
                     ++col)
                {
                    if (col != 0)
                        out << " ";

                    out << block.integers[
                        row * block.cols + col];
                }

                out << "\n";
            }
        }
        else if (block.type_code == 2)
        {
            if (block.reals.size() !=
                block.rows * block.cols)
            {
                throw std::runtime_error(
                    "Invalid real block size: " +
                    block.name);
            }

            for (std::size_t row = 0;
                 row < block.rows;
                 ++row)
            {
                for (std::size_t col = 0;
                     col < block.cols;
                     ++col)
                {
                    if (col != 0)
                        out << " ";

                    out << block.reals[
                        row * block.cols + col];
                }

                out << "\n";
            }
        }
        else
        {
            for (const auto& raw_line :
                 block.raw_lines)
            {
                out << raw_line << "\n";
            }
        }

        out << "\n";
    }

    if (!out)
    {
        throw std::runtime_error(
            "Failed while writing FPD file: " + path);
    }
}

static bool fpd_blocks_equal(
    const FpdBlock& a,
    const FpdBlock& b)
{
    return
        a.name == b.name &&
        a.type_code == b.type_code &&
        a.rows == b.rows &&
        a.cols == b.cols &&
        a.integers == b.integers &&
        a.reals == b.reals &&
        a.raw_lines == b.raw_lines;
}

static void require_integer_block(
    const FpdBlock& block,
    std::size_t columns)
{
    if (block.type_code != 1 ||
        block.cols != columns ||
        block.integers.size() !=
            block.rows * block.cols)
    {
        throw std::runtime_error(
            block.name +
            " must be an integer block with " +
            std::to_string(columns) +
            " columns");
    }
}

static void require_real_block(
    const FpdBlock& block)
{
    if (block.type_code != 2 ||
        block.reals.size() !=
            block.rows * block.cols)
    {
        throw std::runtime_error(
            block.name +
            " must be a valid real block");
    }
}

static void require_same_block_across_inputs(
    const std::vector<FpdFile>& fpds,
    const std::string& name)
{
    const FpdBlock* reference = nullptr;

    for (const auto& fpd : fpds)
    {
        if (fpd.has(name))
        {
            reference = &fpd.at(name);
            break;
        }
    }

    // Block does not exist in any input.
    if (reference == nullptr)
        return;

    for (std::size_t i = 0;
         i < fpds.size();
         ++i)
    {
        if (!fpds[i].has(name))
        {
            throw std::runtime_error(
                "FPD block " + name +
                " is missing from input " +
                std::to_string(i));
        }

        if (!fpd_blocks_equal(
                *reference,
                fpds[i].at(name)))
        {
            throw std::runtime_error(
                "FPD block " + name +
                " differs between inputs");
        }
    }
}

static bool is_managed_fpd_block(
    const std::string& name)
{
    return
        name == "RRMPANALTYPE" ||
        name == "RRMPROTAXIS" ||
        name == "RRMPDOMAIN" ||
        name == "RRMPDOMPROPS" ||
        name == "RRMPBNDPROPS" ||
        name == "RRMPRAMP" ||
        name == "RRMPRAMPTIME" ||
        name == "RRMPBDD" ||
        name == "RRMPNLSOL" ||
        name == "RRMPORDER" ||
        name == "RRMPDOMOUT";
}

static void require_unmanaged_blocks_identical(
    const std::vector<FpdFile>& fpds)
{
    std::unordered_map<std::string, int> names;

    for (const auto& fpd : fpds)
    {
        for (const auto& block : fpd.blocks)
        {
            if (!is_managed_fpd_block(block.name))
                names.emplace(block.name, 0);
        }
    }

    for (const auto& item : names)
    {
        require_same_block_across_inputs(
            fpds,
            item.first);
    }
}

static long long shift_bdd_pointer(
    long long pointer,
    long long parameter_count,
    std::size_t source_bdd_rows,
    std::size_t bdd_row_offset,
    const std::string& context)
{
    if (pointer == 0)
        return 0;

    if (pointer < 1 ||
        parameter_count < 0)
    {
        throw std::runtime_error(
            "Invalid BDD pointer in " + context);
    }

    const long long last_row =
        parameter_count > 0
            ? pointer + parameter_count - 1
            : pointer;

    if (last_row < pointer ||
        static_cast<std::size_t>(last_row) >
            source_bdd_rows)
    {
        throw std::runtime_error(
            "BDD pointer range is outside RRMPBDD in " +
            context);
    }

    return pointer +
        static_cast<long long>(bdd_row_offset);
}

static FpdFile merge_fpd_files(
    const std::vector<FpdFile>& fpds,
    const std::vector<InputMarkerMap>& marker_maps,
    const std::unordered_map<
        long long,
        long long>& facet_intermediate_to_final,
    std::size_t merged_domain_rows)
{
    if (fpds.empty())
    {
        throw std::runtime_error(
            "No FPD files were supplied");
    }

    if (fpds.size() != marker_maps.size())
    {
        throw std::runtime_error(
            "FPD count does not match mesh input count");
    }

    // Global analysis settings must match.
    require_same_block_across_inputs(
        fpds,
        "RRMPANALTYPE");

    require_same_block_across_inputs(
        fpds,
        "RRMPROTAXIS");

    require_same_block_across_inputs(
        fpds,
        "RRMPRAMP");

    require_same_block_across_inputs(
        fpds,
        "RRMPRAMPTIME");

    require_same_block_across_inputs(
        fpds,
        "RRMPNLSOL");

    require_same_block_across_inputs(
        fpds,
        "RRMPORDER");

    // Material, expression, interpolation and string blocks are
    // currently retained only when identical in every input.
    require_unmanaged_blocks_identical(fpds);

    FpdFile merged = fpds.front();

    // ------------------------------------------------------------
    // Merge RRMPBDD vertically.
    // ------------------------------------------------------------

    std::vector<std::size_t> bdd_offsets(
        fpds.size(),
        0);

    std::vector<std::size_t> bdd_rows(
        fpds.size(),
        0);

    bool any_bdd = false;

    for (const auto& fpd : fpds)
    {
        if (fpd.has("RRMPBDD"))
            any_bdd = true;
    }

    if (any_bdd)
    {
        for (std::size_t i = 0;
             i < fpds.size();
             ++i)
        {
            if (!fpds[i].has("RRMPBDD"))
            {
                throw std::runtime_error(
                    "RRMPBDD is missing from input " +
                    std::to_string(i));
            }
        }

        const FpdBlock& first_bdd =
            fpds.front().at("RRMPBDD");

        require_real_block(first_bdd);

        FpdBlock merged_bdd = first_bdd;

        merged_bdd.rows = 0;
        merged_bdd.integers.clear();
        merged_bdd.reals.clear();
        merged_bdd.raw_lines.clear();

        std::size_t accumulated_rows = 0;

        for (std::size_t i = 0;
             i < fpds.size();
             ++i)
        {
            const FpdBlock& source =
                fpds[i].at("RRMPBDD");

            require_real_block(source);

            if (source.cols != first_bdd.cols)
            {
                throw std::runtime_error(
                    "RRMPBDD condition count differs in input " +
                    std::to_string(i));
            }

            bdd_offsets[i] = accumulated_rows;
            bdd_rows[i] = source.rows;

            merged_bdd.reals.insert(
                merged_bdd.reals.end(),
                source.reals.begin(),
                source.reals.end());

            accumulated_rows += source.rows;
        }

        merged_bdd.rows = accumulated_rows;
        merged.set(merged_bdd);
    }

    // ------------------------------------------------------------
    // Merge RRMPDOMAIN using the HDF5 volume-marker mapping.
    // ------------------------------------------------------------

    for (std::size_t i = 0;
         i < fpds.size();
         ++i)
    {
        if (!fpds[i].has("RRMPDOMAIN"))
        {
            throw std::runtime_error(
                "RRMPDOMAIN is missing from input " +
                std::to_string(i));
        }
    }

    FpdBlock merged_domain =
        fpds.front().at("RRMPDOMAIN");

    require_integer_block(
        merged_domain,
        8);

    merged_domain.rows =
        merged_domain_rows;

    merged_domain.integers.assign(
        merged_domain_rows * 8,
        0);

    std::vector<char> domain_written(
        merged_domain_rows,
        0);

    for (std::size_t file_index = 0;
         file_index < fpds.size();
         ++file_index)
    {
        const FpdBlock& source =
            fpds[file_index].at("RRMPDOMAIN");

        require_integer_block(
            source,
            8);

        for (const auto& mapping :
             marker_maps[file_index]
                 .volume_old_to_final)
        {
            const long long old_marker =
                mapping.first;

            const long long new_marker =
                mapping.second;

            if (old_marker < 1 ||
                static_cast<std::size_t>(old_marker) >
                    source.rows)
            {
                throw std::runtime_error(
                    "Volume marker " +
                    std::to_string(old_marker) +
                    " has no RRMPDOMAIN row in input " +
                    std::to_string(file_index));
            }

            if (new_marker < 1 ||
                static_cast<std::size_t>(new_marker) >
                    merged_domain_rows)
            {
                throw std::runtime_error(
                    "Invalid merged volume marker");
            }

            const std::size_t source_row =
                static_cast<std::size_t>(
                    old_marker - 1);

            const std::size_t target_row =
                static_cast<std::size_t>(
                    new_marker - 1);

            if (domain_written[target_row])
            {
                throw std::runtime_error(
                    "Multiple source domains map to merged marker " +
                    std::to_string(new_marker));
            }

            for (std::size_t col = 0;
                 col < 8;
                 ++col)
            {
                merged_domain.integers[
                    target_row * 8 + col] =
                    source.integers[
                        source_row * 8 + col];
            }

            domain_written[target_row] = 1;
        }
    }

    merged.set(merged_domain);

    // ------------------------------------------------------------
    // Merge RRMPDOMPROPS and shift its BDD pointers.
    // ------------------------------------------------------------

    bool any_domprops = false;

    for (const auto& fpd : fpds)
    {
        if (fpd.has("RRMPDOMPROPS"))
            any_domprops = true;
    }

    if (any_domprops)
    {
        for (std::size_t i = 0;
             i < fpds.size();
             ++i)
        {
            if (!fpds[i].has("RRMPDOMPROPS"))
            {
                throw std::runtime_error(
                    "RRMPDOMPROPS is missing from input " +
                    std::to_string(i));
            }
        }

        FpdBlock merged_domprops =
            fpds.front().at("RRMPDOMPROPS");

        require_integer_block(
            merged_domprops,
            5);

        merged_domprops.rows =
            merged_domain_rows;

        merged_domprops.integers.assign(
            merged_domain_rows * 5,
            0);

        const long long parameter_counts[5] =
        {
            7, // rotational-speed parameters
            7, // current-density parameters
            1, // magnetisation
            1, // structural-domain temperature
            1  // internal heat generation
        };

        for (std::size_t file_index = 0;
             file_index < fpds.size();
             ++file_index)
        {
            const FpdBlock& source =
                fpds[file_index]
                    .at("RRMPDOMPROPS");

            require_integer_block(
                source,
                5);

            for (const auto& mapping :
                 marker_maps[file_index]
                     .volume_old_to_final)
            {
                const long long old_marker =
                    mapping.first;

                const long long new_marker =
                    mapping.second;

                if (old_marker < 1 ||
                    static_cast<std::size_t>(old_marker) >
                        source.rows)
                {
                    throw std::runtime_error(
                        "Volume marker has no "
                        "RRMPDOMPROPS row");
                }

                const std::size_t source_row =
                    static_cast<std::size_t>(
                        old_marker - 1);

                const std::size_t target_row =
                    static_cast<std::size_t>(
                        new_marker - 1);

                for (std::size_t col = 0;
                     col < 5;
                     ++col)
                {
                    merged_domprops.integers[
                        target_row * 5 + col] =
                        shift_bdd_pointer(
                            source.integers[
                                source_row * 5 + col],
                            parameter_counts[col],
                            bdd_rows[file_index],
                            bdd_offsets[file_index],
                            "RRMPDOMPROPS input " +
                                std::to_string(
                                    file_index));
                }
            }
        }

        merged.set(merged_domprops);
    }

    // ------------------------------------------------------------
    // Merge optional RRMPDOMOUT.
    // ------------------------------------------------------------

    bool any_domout = false;

    for (const auto& fpd : fpds)
    {
        if (fpd.has("RRMPDOMOUT"))
            any_domout = true;
    }

    if (any_domout)
    {
        for (std::size_t i = 0;
             i < fpds.size();
             ++i)
        {
            if (!fpds[i].has("RRMPDOMOUT"))
            {
                throw std::runtime_error(
                    "RRMPDOMOUT is missing from input " +
                    std::to_string(i));
            }
        }

        FpdBlock merged_domout =
            fpds.front().at("RRMPDOMOUT");

        require_integer_block(
            merged_domout,
            4);

        merged_domout.rows =
            merged_domain_rows;

        merged_domout.integers.assign(
            merged_domain_rows * 4,
            0);

        for (std::size_t file_index = 0;
             file_index < fpds.size();
             ++file_index)
        {
            const FpdBlock& source =
                fpds[file_index].at("RRMPDOMOUT");

            require_integer_block(
                source,
                4);

            for (const auto& mapping :
                 marker_maps[file_index]
                     .volume_old_to_final)
            {
                const long long old_marker =
                    mapping.first;

                const long long new_marker =
                    mapping.second;

                if (old_marker < 1 ||
                    static_cast<std::size_t>(old_marker) >
                        source.rows)
                {
                    throw std::runtime_error(
                        "Volume marker has no RRMPDOMOUT row");
                }

                const std::size_t source_row =
                    static_cast<std::size_t>(
                        old_marker - 1);

                const std::size_t target_row =
                    static_cast<std::size_t>(
                        new_marker - 1);

                for (std::size_t col = 0;
                     col < 4;
                     ++col)
                {
                    merged_domout.integers[
                        target_row * 4 + col] =
                        source.integers[
                            source_row * 4 + col];
                }
            }
        }

        merged.set(merged_domout);
    }

    // ------------------------------------------------------------
    // Merge RRMPBNDPROPS using surviving HDF5 facet markers.
    // ------------------------------------------------------------

    bool any_bndprops = false;
    std::size_t first_bndprops_input = 0;

    for (std::size_t i = 0;
         i < fpds.size();
         ++i)
    {
        if (fpds[i].has("RRMPBNDPROPS"))
        {
            any_bndprops = true;
            first_bndprops_input = i;
            break;
        }
    }

    if (any_bndprops)
    {
        FpdBlock merged_bndprops =
            fpds[first_bndprops_input]
                .at("RRMPBNDPROPS");

        require_integer_block(
            merged_bndprops,
            6);

        merged_bndprops.rows = 0;
        merged_bndprops.integers.clear();
        merged_bndprops.reals.clear();
        merged_bndprops.raw_lines.clear();

        for (std::size_t file_index = 0;
             file_index < fpds.size();
             ++file_index)
        {
            if (!fpds[file_index]
                     .has("RRMPBNDPROPS"))
            {
                continue;
            }

            const FpdBlock& source =
                fpds[file_index]
                    .at("RRMPBNDPROPS");

            require_integer_block(
                source,
                6);

            for (std::size_t row = 0;
                 row < source.rows;
                 ++row)
            {
                long long values[6];

                for (std::size_t col = 0;
                     col < 6;
                     ++col)
                {
                    values[col] =
                        source.integers[
                            row * 6 + col];
                }

                const long long old_face_marker =
                    values[4];

                const auto intermediate_it =
                    marker_maps[file_index]
                        .facet_old_to_intermediate
                        .find(old_face_marker);

                if (intermediate_it ==
                    marker_maps[file_index]
                        .facet_old_to_intermediate
                        .end())
                {
                    throw std::runtime_error(
                        "RRMPBNDPROPS face marker " +
                        std::to_string(old_face_marker) +
                        " is absent from HDF5 facet values "
                        "in input " +
                        std::to_string(file_index));
                }

                const auto final_it =
                    facet_intermediate_to_final.find(
                        intermediate_it->second);

                if (final_it ==
                    facet_intermediate_to_final.end())
                {
                    // All triangles carrying this marker became
                    // conformal internal facets and were removed.
                    continue;
                }

                // Column 4 in the FPD description:
                // pointer to the first BDD parameter row.
                values[3] =
                    shift_bdd_pointer(
                        values[3],
                        values[2],
                        bdd_rows[file_index],
                        bdd_offsets[file_index],
                        "RRMPBNDPROPS input " +
                            std::to_string(
                                file_index));

                // Column 5:
                // HDF5 facet-marker number.
                values[4] = final_it->second;

                for (std::size_t col = 0;
                     col < 6;
                     ++col)
                {
                    merged_bndprops.integers
                        .push_back(values[col]);
                }
            }
        }

        merged_bndprops.rows =
            merged_bndprops.integers.size() / 6;

        // Input files commonly restart the boundary-condition index
        // column at one. Rebuild it for the merged file per BC type.
        std::unordered_map<
            long long,
            long long> next_index_by_type;

        for (std::size_t row = 0;
             row < merged_bndprops.rows;
             ++row)
        {
            const long long type =
                merged_bndprops.integers[
                    row * 6];

            merged_bndprops.integers[
                row * 6 + 1] =
                ++next_index_by_type[type];
        }

        merged.set(merged_bndprops);
    }

    return merged;
}
