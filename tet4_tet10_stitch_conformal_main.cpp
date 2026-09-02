#include <H5Cpp.h>
extern "C" {
  #include <hdf5.h>
  #include <H5Lpublic.h>
}
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#ifdef USE_TINYXML2
    #include "tinyxml2.h"
    using namespace tinyxml2;
#endif

using namespace H5;

// -------- dataset paths --------
const std::string PATH_NODES = "/Geometry/geometry/geometry";
const std::string PATH_ELEM_TOPO = "/MeshTags/volume markers/topology"; // Tet4 (M x 4) or Tet10 (M x 10)
const std::string PATH_ELEM_VALUES = "/MeshTags/volume markers/Values"; // (M), (Mx1), or (1xM)
const std::string PATH_FACET_TOPO = "/MeshTags/facet markers/topology"; // Tri3 (F x 3) or Tri6 (F x 6) [optional]
const std::string PATH_FACET_VALUES = "/MeshTags/facet markers/Values"; // (F), (Fx1), or (1xF) [optional]



static bool dbg_on() {
    static int on = []{
        const char* e = std::getenv("MERGER_DEBUG");
        return (e && *e && std::string(e)!="0") ? 1 : 0;
    }();
    return on;
}
static void dbg_shape(const char* label, const std::string& path, const DataSpace& s) {
    if (!dbg_on()) return;
    int rank = s.getSimpleExtentNdims();
    std::vector<hsize_t> dims(rank, 0);
    if (rank>0) s.getSimpleExtentDims(dims.data());
    hssize_t npts = s.getSimpleExtentNpoints();
    std::cerr << "[MERGER] " << label << " " << path << " | rank="<<rank<<" dims=";
    for (int i=0;i<rank;++i) std::cerr << (i?",":"") << dims[i];
    std::cerr << " | npoints="<< npts << "\n";
}

// -------- File info --------
struct FileInfo {
    std::string path;
    hsize_t num_nodes = 0;
    hsize_t num_elems = 0;
    hsize_t num_facets = 0;
    int elem_index_base = 0; // detected 0 or 1
    int facet_index_base = 0; // detected 0 or 1
    hsize_t nodes_per_elem = 0;
    hsize_t nodes_per_facet = 0;
};

// -------- Utilities --------
static bool dataset_exists(const H5File& f, const std::string& path) {
    try { f.openDataSet(path); return true; }
    catch (...) { return false; }
}

// -------- Safe readers --------
static void read_nodes_safe(const H5File& f, const std::string& path,
                            std::vector<double>& nodes_xyz, hsize_t& N) {
    DataSet d = f.openDataSet(path);
    DataSpace s = d.getSpace();
    dbg_shape("open", path, s);
    if (s.getSimpleExtentNdims()!=2) throw std::runtime_error("nodes must be rank-2: "+path);
    hsize_t dims[2]; s.getSimpleExtentDims(dims);
    if (dims[1]!=3) throw std::runtime_error("nodes second dim must be 3 at: "+path);
    N = dims[0];
    nodes_xyz.resize(static_cast<size_t>(N)*3);
    if (s.getSimpleExtentNpoints()==0) return; // N==0
    d.read(nodes_xyz.data(), PredType::NATIVE_DOUBLE); // auto-convert if float in file
}

static void detect_index_base(const std::vector<long long>& idx,
                              hsize_t Nnodes, int& index_base) {
    const long long N = static_cast<long long>(Nnodes);
    bool fits0=true, fits1=true;
    for (auto v: idx) { if(!(0<=v && v<N)) fits0=false; if(!(1<=v && v<=N)) fits1=false; }
    index_base = fits0 ? 0 : (fits1 ? 1 : 0);
}

static void read_elem_topology(const H5File& f, const std::string& path, hsize_t Nnodes,
                               std::vector<long long>& conn, hsize_t& M,
                               hsize_t& nodes_per_elem, int& index_base) {
    DataSet d = f.openDataSet(path);
    DataSpace s = d.getSpace();
    dbg_shape("open", path, s);
    if (s.getSimpleExtentNdims()!=2) throw std::runtime_error("element topology must be rank-2: "+path);
    hsize_t dims[2]; s.getSimpleExtentDims(dims);
    if (dims[1]!=4 && dims[1]!=10)
        throw std::runtime_error("Supported volume topology widths are 4 (Tet4) and 10 (Tet10) at: "+path);
    M = dims[0];
    nodes_per_elem = dims[1];
    conn.resize(static_cast<size_t>(M)*static_cast<size_t>(nodes_per_elem));
    if (s.getSimpleExtentNpoints()==0) { index_base=0; return; }
    d.read(conn.data(), PredType::NATIVE_LLONG);
    detect_index_base(conn, Nnodes, index_base);
}

static void read_facet_topology(const H5File& f, const std::string& path, hsize_t Nnodes,
                                std::vector<long long>& conn, hsize_t& F,
                                hsize_t& nodes_per_facet, int& index_base) {
    DataSet d = f.openDataSet(path);
    DataSpace s = d.getSpace();
    dbg_shape("open", path, s);
    if (s.getSimpleExtentNdims()!=2) throw std::runtime_error("facet topology must be rank-2: "+path);
    hsize_t dims[2]; s.getSimpleExtentDims(dims);
    if (dims[1]!=3 && dims[1]!=6)
        throw std::runtime_error("Supported facet topology widths are 3 (Tri3) and 6 (Tri6) at: "+path);
    F = dims[0];
    nodes_per_facet = dims[1];
    conn.resize(static_cast<size_t>(F)*static_cast<size_t>(nodes_per_facet));
    if (s.getSimpleExtentNpoints()==0) { index_base=0; return; }
    d.read(conn.data(), PredType::NATIVE_LLONG);
    detect_index_base(conn, Nnodes, index_base);
}

static void read_values_int64_flexible(const DataSet& d, hsize_t L, std::vector<long long>& out) 
{
    DataSpace s = d.getSpace();
    dbg_shape("open", "<values>", s);
    int rank = s.getSimpleExtentNdims();
    std::vector<hsize_t> dims(rank,0);
    if (rank>0) s.getSimpleExtentDims(dims.data());
    if (s.getSimpleExtentNpoints()==0) { out.clear(); return; } // L==0

    if (rank==1) {
        if (dims[0]!=L) throw std::runtime_error("Values length mismatch");
    } else if (rank==2) {
        if (!((dims[0]==L && dims[1]==1) || (dims[0]==1 && dims[1]==L)))
            throw std::runtime_error("Values must be (L), (Lx1) or (1xL)");
    } else {
        throw std::runtime_error("Values rank must be 1 or 2");
    }
    out.resize(static_cast<size_t>(L));
    d.read(out.data(), PredType::NATIVE_LLONG);
}

static void read_element_values(const H5File& f, const std::string& path, hsize_t M,
                                std::vector<long long>& vals) 
{
    DataSet d = f.openDataSet(path);
    read_values_int64_flexible(d, M, vals);
}
static void read_facet_values(const H5File& f, const std::string& path, hsize_t F,
                              std::vector<long long>& vals) {
    DataSet d = f.openDataSet(path);
    read_values_int64_flexible(d, F, vals);
}

// -------- Voxel (bucket) merger, Chebyshev = e --------
struct CellKey {int group; long long ix, iy, iz; };
static inline bool operator==(const CellKey& a, const CellKey& b){
    return a.group == b.group && a.ix==b.ix && a.iy==b.iy && a.iz==b.iz;
}
struct CellKeyHash {
    std::size_t operator()(const CellKey& k) const noexcept {
        std::size_t h = std::hash<int>()(k.group);
		h ^= std::hash<long long>()(k.ix + 0x9e3779b97f4a7c15ULL + (h<<6) + (h>>2));
        h ^= std::hash<long long>()(k.iy + 0x9e3779b97f4a7c15ULL + (h<<6) + (h>>2));
        h ^= std::hash<long long>()(k.iz + 0x9e3779b97f4a7c15ULL + (h<<6) + (h>>2));
        return h;
    }
};

class NodeMerger {
public:
    explicit NodeMerger(double eps): eps_(eps), inv_(1.0/eps) {
        if (!(eps_>0.0)) throw std::runtime_error("epsilon must be > 0");
    }
    size_t upsert(double x,double y,double z, int group, int file_id){
        const CellKey c = key_for(x,y,z,group);
        for (long long dx=-1; dx<=1; ++dx)
        for (long long dy=-1; dy<=1; ++dy)
        for (long long dz=-1; dz<=1; ++dz) {
            auto it = grid_.find(CellKey{group,c.ix+dx,c.iy+dy,c.iz+dz});
            if (it==grid_.end()) continue;
            for (size_t id : it->second) {
                const double X = nodes_[3*id+0];
                const double Y = nodes_[3*id+1];
                const double Z = nodes_[3*id+2];
                if (node_file_[id] != file_id && std::fabs(X-x)<=eps_ && std::fabs(Y-y)<=eps_ && std::fabs(Z-z)<=eps_)
                    return id; // duplicate
            }
        }
        const size_t id = nodes_.size()/3;
        nodes_.push_back(x); nodes_.push_back(y); nodes_.push_back(z);
		node_file_.push_back(file_id);
        grid_[c].push_back(id);
        return id;
    }
    const std::vector<double>& nodes_xyz() const { return nodes_; }
    hsize_t count() const { return static_cast<hsize_t>(nodes_.size()/3); }
private:
    CellKey key_for(double x,double y,double z, int group) const {
        return CellKey{
			group,
            static_cast<long long>(std::floor(x*inv_)),
            static_cast<long long>(std::floor(y*inv_)),
            static_cast<long long>(std::floor(z*inv_))
        };
    }
    double eps_, inv_;
    std::unordered_map<CellKey, std::vector<size_t>, CellKeyHash> grid_;
    std::vector<double> nodes_; // [x0,y0,z0, x1,y1,z1, ...]
	std::vector<int> node_file_;
};

// -------- Writer helpers --------
static void ensure_parents(H5File& out, const std::string& full_path) {
    if (full_path.empty() || full_path[0]!='/') return;
    std::string cur;
    size_t start = 1;
    while (true) {
        size_t slash = full_path.find('/', start);
        if (slash == std::string::npos) break;
        cur = full_path.substr(0, slash);
        if (!cur.empty() && cur!="/") {
            if (H5Lexists(out.getId(), cur.c_str(), H5P_DEFAULT) <= 0)
                out.createGroup(cur);
        }
        start = slash + 1;
    }
}

static void write_output(const std::string& out_path,
                         const std::vector<double>& nodes_xyz,
                         const std::vector<long long>& elem_conn,
                         const std::vector<long long>& elem_vals,
                         const std::vector<long long>& facet_conn,
                         const std::vector<long long>& facet_vals,
                         hsize_t N_unique, hsize_t M_total, hsize_t F_total,
                         hsize_t nodes_per_elem, hsize_t nodes_per_facet,
                         const std::vector<FileInfo>& inputs,
                         double epsilon_used)
{
    H5File out(out_path, H5F_ACC_TRUNC);
    for (const std::string& p : {PATH_NODES, PATH_ELEM_TOPO, PATH_ELEM_VALUES, PATH_FACET_TOPO, PATH_FACET_VALUES})
        ensure_parents(out, p);

    // Nodes (N x 3)
    {
        hsize_t dims[2] = { N_unique, 3 };
        DataSpace sp(2, dims);
        DataSet ds = out.createDataSet(PATH_NODES, PredType::NATIVE_DOUBLE, sp);
        if (N_unique>0) ds.write(nodes_xyz.data(), PredType::NATIVE_DOUBLE);
    }
    // Tet4/Tet10 topology
    {
        hsize_t dims[2] = { M_total, nodes_per_elem };
        DataSpace sp(2, dims);
        DataSet ds = out.createDataSet(PATH_ELEM_TOPO, PredType::NATIVE_LLONG, sp);
        if (!elem_conn.empty()) ds.write(elem_conn.data(), PredType::NATIVE_LLONG);
        StrType st(PredType::C_S1, H5T_VARIABLE);
        Attribute a = ds.createAttribute("index_base", st, DataSpace(H5S_SCALAR));
        const char* v = "0"; a.write(st, &v);
    }
    // Volume values (M x 1)
    {
        hsize_t dims[2] = { M_total, 1 };
        DataSpace sp(2, dims);
        DataSet ds = out.createDataSet(PATH_ELEM_VALUES, PredType::NATIVE_LLONG, sp);
        if (!elem_vals.empty()) ds.write(elem_vals.data(), PredType::NATIVE_LLONG);
    }
    // Tri3/Tri6 topology
    {
        hsize_t dims[2] = { F_total, nodes_per_facet };
        DataSpace sp(2, dims);
        DataSet ds = out.createDataSet(PATH_FACET_TOPO, PredType::NATIVE_LLONG, sp);
        if (!facet_conn.empty()) ds.write(facet_conn.data(), PredType::NATIVE_LLONG);
        StrType st(PredType::C_S1, H5T_VARIABLE);
        Attribute a = ds.createAttribute("index_base", st, DataSpace(H5S_SCALAR));
        const char* v = "0"; a.write(st, &v);
    }
    // Facet values (F x 1)
    {
        hsize_t dims[2] = { F_total, 1 };
        DataSpace sp(2, dims);
        DataSet ds = out.createDataSet(PATH_FACET_VALUES, PredType::NATIVE_LLONG, sp);
        if (!facet_vals.empty()) ds.write(facet_vals.data(), PredType::NATIVE_LLONG);
    }

    // Provenance under /MeshTags if exists, else /
    Group where = out.openGroup("/");
    try { where = out.openGroup("/MeshTags"); } catch (...) {}
    {
        Attribute ae = where.createAttribute("merge_epsilon", PredType::NATIVE_DOUBLE, DataSpace(H5S_SCALAR));
        ae.write(PredType::NATIVE_DOUBLE, &epsilon_used);

        const hsize_t n = inputs.size();
        StrType st(PredType::C_S1, H5T_VARIABLE); DataSpace sp(1, &n);

        std::vector<const char*> files; files.reserve(n);
        std::vector<std::string> eb(n), fb(n);
        std::vector<const char*> eb_c, fb_c; eb_c.reserve(n); fb_c.reserve(n);

        for (const auto& fi : inputs) files.push_back(fi.path.c_str());
        for (size_t i=0;i<n;++i){ eb[i] = inputs[i].elem_index_base ? "1":"0"; eb_c.push_back(eb[i].c_str()); }
        for (size_t i=0;i<n;++i){ fb[i] = inputs[i].facet_index_base? "1":"0"; fb_c.push_back(fb[i].c_str()); }

        Attribute af = where.createAttribute("input_files", st, sp); af.write(st, files.data());
        Attribute aeb = where.createAttribute("input_detected_index_bases_elements", st, sp); aeb.write(st, eb_c.data());
        Attribute afb = where.createAttribute("input_detected_index_bases_facets", st, sp); afb.write(st, fb_c.data());
    }

    std::cout << "Wrote merged mesh ? " << out_path << "\n"
              << " nodes: " << N_unique
              << " elements: " << M_total << " (Tet" << nodes_per_elem << ")\n"
              << " facets: " << F_total << " (Tri" << nodes_per_facet << ")\n";

    #ifdef USE_TINYXML2
        std::ostringstream n;
        std::ostringstream m;
        std::ostringstream f;
        std::ostringstream dimsNode1;
        std::ostringstream dimsVol1;
        std::ostringstream dimsVol2;
        std::ostringstream dimsFacet1;
        std::ostringstream dimsFacet2;

        n << N_unique;
        m << M_total;
        f << F_total;

        const std::string nstr = n.str();
        const std::string mstr = m.str();
        const std::string fstr = f.str();

        dimsVol1 << M_total << " " << nodes_per_elem;
        const std::string dimsVol1str = dimsVol1.str();
        dimsVol2 << M_total << " " << 1;
        const std::string dimsVol2str = dimsVol2.str();
        dimsNode1 << N_unique << " " << 3;
        const std::string dimsNode1str = dimsNode1.str();
        dimsFacet1 << F_total << " " << nodes_per_facet;
        const std::string dimsFacet1str = dimsFacet1.str();
        dimsFacet2 <<F_total << " " << 1;
        const std::string dimsFacet2str = dimsFacet2.str();

        XMLDocument doc;
        doc.InsertFirstChild(doc.NewDeclaration());
        XMLUnknown * doctype = doc.NewUnknown("DOCTYPE Xdmf SYSTEM \"Xdmf.dtd\" []");
        doc.InsertEndChild(doctype);
        XMLElement * root  = doc.NewElement("Xdmf");
        root -> SetAttribute("Version", "3.0");
        root -> SetAttribute("xmlns:xi", "http://www.w3.org/2001/XInclude");
        doc.InsertEndChild(root);
        
        XMLElement * domain = doc.NewElement("Domain");
        XMLElement * grid1 = doc.NewElement("Grid");
        XMLElement * geom = doc.NewElement("Geometry");
        XMLElement * dataitem1 = doc.NewElement("DataItem");
        XMLElement * grid2 = doc.NewElement("Grid");
        XMLElement * xi1 = doc.NewElement("xi:include");
        XMLElement * topo1 = doc.NewElement("Topology");
        XMLElement * dataitem2 = doc.NewElement("DataItem");
        XMLElement * attr1 = doc.NewElement("Attribute");
        XMLElement * dataitem3 = doc.NewElement("DataItem");
        XMLElement * grid3 = doc.NewElement("Grid");
        XMLElement * xi2 = doc.NewElement("xi:include");
        XMLElement * topo2 = doc.NewElement("Topology");
        XMLElement * dataitem4 = doc.NewElement("DataItem");
        XMLElement * attr2 = doc.NewElement("Attribute");
        XMLElement * dataitem5 = doc.NewElement("DataItem");
        
        root -> InsertEndChild(domain);
        domain -> InsertEndChild(grid1);
        domain -> InsertEndChild(grid2);
        domain -> InsertEndChild(grid3);
        grid1 -> InsertEndChild(geom);
        grid2 -> InsertEndChild(xi1);
        grid2 -> InsertEndChild(topo1);
        grid2 -> InsertEndChild(attr1);
        grid3 -> InsertEndChild(xi2);
        grid3 -> InsertEndChild(topo2);
        grid3 -> InsertEndChild(attr2);
        geom -> InsertEndChild(dataitem1);
        topo1 -> InsertEndChild(dataitem2);
        attr1 -> InsertEndChild(dataitem3);
        topo2 -> InsertEndChild(dataitem4);
        attr2 -> InsertEndChild(dataitem5);

        grid1 -> SetAttribute("Name", "geometry");
        grid1 -> SetAttribute("GridType", "Uniform");
        geom -> SetAttribute("GeometryType", "XYZ");
        dataitem1 -> SetAttribute("Dimensions", dimsNode1str.c_str());
        dataitem1 -> SetAttribute("Format", "HDF");
        dataitem1 -> SetText("merged.h5:/Geometry/geometry/geometry");
        grid2 -> SetAttribute("Name", "volume markers");
        grid2 -> SetAttribute("GridType", "Uniform");
        xi1 -> SetAttribute("xpointer", "xpointer(/Xdmf/Domain/Grid[@Name='geometry']/Geometry)");
        topo1 -> SetAttribute("TopologyType", nodes_per_elem == 10 ? "Tetrahedron_10" : "Tetrahedron");
        topo1 -> SetAttribute("NumberOfElements", mstr.c_str());
        topo1 -> SetAttribute("NodesPerElement", static_cast<unsigned int>(nodes_per_elem));
        dataitem2 -> SetAttribute("Dimensions", dimsVol1str.c_str());
        dataitem2 -> SetAttribute("NumberType", "Int");
        dataitem2 -> SetAttribute("Format", "HDF");
        dataitem2 -> SetText("merged.h5:/MeshTags/volume markers/topology");
        attr1 -> SetAttribute("Name", "volume markers");
        attr1 -> SetAttribute("AttributeType", "Scalar");
        attr1 -> SetAttribute("Center", "Cell");
        dataitem3 -> SetAttribute("Dimensions", dimsVol2str.c_str());
        dataitem3 -> SetAttribute("Format", "HDF");
        dataitem3 -> SetText("merged.h5:/MeshTags/volume markers/Values");
        grid3 -> SetAttribute("Name", "facet markers");
        grid3 -> SetAttribute("GridType", "Uniform");
        xi2 -> SetAttribute("xpointer", "xpointer(/Xdmf/Domain/Grid[@Name='geometry']/Geometry)");
        topo2 -> SetAttribute("TopologyType", nodes_per_facet == 6 ? "Triangle_6" : "Triangle");
        topo2 -> SetAttribute("NumberOfElements", fstr.c_str());
        topo2 -> SetAttribute("NodesPerElement", static_cast<unsigned int>(nodes_per_facet));
        dataitem4 -> SetAttribute("Dimensions", dimsFacet1str.c_str());
        dataitem4 -> SetAttribute("NumberType", "Int");
        dataitem4 -> SetAttribute("Format", "HDF");
        dataitem4 -> SetText("merged.h5:/MeshTags/facet markers/topology");
        attr2 -> SetAttribute("Name", "facet markers");
        attr2 -> SetAttribute("AttributeType", "Scalar");
        attr2 -> SetAttribute("Center", "Cell");
        dataitem5 -> SetAttribute("Dimensions", dimsFacet2str.c_str());
        dataitem5 -> SetAttribute("Format", "HDF"); 
        dataitem5 -> SetText("merged.h5:/MeshTags/facet markers/Values");

        doc.SaveFile("merged.xdmf");
    #endif
}

struct InputSpec{
	std::string path;
	int group;
};

static bool is_conformal_token(const std::string& s){
	return s=="c" || s=="conformal";
}

static bool is_nonconformal_token(const std::string& s){
	return s=="n" || s=="nonconformal";
}

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
// -------- main --------
int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr <<
          "Usage:\n  " << argv[0] <<
          "<epsilon> inp1.h5 c inp2.h5 n inp3.h5 ...\n";
        return 1;
    }
 
    const std::string out_path = "merged.h5";
    const double epsilon = std::stod(argv[1]);
	std::vector<InputSpec> inputs;
	std::vector<int> all_node_file_ids;
	
	int current_group = 0;
	InputSpec first;
	first.path = argv[2];
	first.group = current_group;
	inputs.push_back(first);
	int argi = 3;
	while(argi < argc)
	{
		if (argi+1 >= argc)
			throw std::runtime_error("Expected relation token followed by h5 file");
		std::string rel = argv[argi];
		std::string next_file = argv[argi+1];
		if(is_conformal_token(rel))
		{
			
		}
		else if(is_nonconformal_token(rel))
		{
			++current_group;
		}
		else{
			throw std::runtime_error("Expected token c or n between files, got " + rel);
		}
		InputSpec spec;
		spec.path = next_file;
		spec.group = current_group;
		inputs.push_back(spec);
		argi += 2;
		
	}
	
	std::cout<<"Parsed " << inputs.size() << " input files\n";
	for(size_t i=0; i<inputs.size(); ++i)
	{
		std::cout<< " [" << i << "] group =" <<inputs[i].group << " file=" << inputs[i].path << "\n";
		std::cout << std::flush;
	}

    try 
    {
    // Silence HDF5's auto error stack
    //H5::Exception::dontPrint();

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

        std::vector<double> all_nodes_xyz;

        std::vector<long long> all_elem_conn, all_elem_vals;
        std::vector<long long> all_facet_conn, all_facet_vals;
        std::vector<FileInfo> file_infos;

        long long node_offset = 0;
        hsize_t totalN=0, totalM=0, totalF=0;
        int global_body_offset = 0;
        int global_facet_offset = 0;
		hsize_t mesh_nodes_per_elem = 0;
		hsize_t mesh_nodes_per_facet = 0;
		
		std::vector<int> all_node_groups;
		int file_id = 0;
        for (const auto& inp : inputs) {
			const std::string& path = inp.path;
            H5File f(path, H5F_ACC_RDONLY);
            FileInfo info; info.path = path;

            // Nodes
            std::vector<double> nodes_xyz; hsize_t N=0;
            read_nodes_safe(f, PATH_NODES, nodes_xyz, N);
            info.num_nodes = N;

            // Tet4 or Tet10 elements. Connectivity order is preserved verbatim.
            std::vector<long long> elem_conn; hsize_t M=0;
            read_elem_topology(f, PATH_ELEM_TOPO, N, elem_conn, M,
                               info.nodes_per_elem, info.elem_index_base);
            info.num_elems = M;
            if (mesh_nodes_per_elem == 0) mesh_nodes_per_elem = info.nodes_per_elem;
            if (info.nodes_per_elem != mesh_nodes_per_elem)
                throw std::runtime_error("Cannot merge Tet4 and Tet10 inputs in one output mesh");

            std::vector<long long> eVals;
            read_element_values(f, PATH_ELEM_VALUES, M, eVals);
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

            if (info.elem_index_base==1) for (auto& v: elem_conn) v -= 1; // 1 to 0
            for (auto& v: elem_conn) v += node_offset; // shift into concat index space

            all_elem_conn.insert(all_elem_conn.end(), elem_conn.begin(), elem_conn.end());
            all_elem_vals.insert(all_elem_vals.end(), eVals.begin(), eVals.end());

            // Facets (Tri3 for Tet4, Tri6 for Tet10) optional.
            bool haveFacetTopo = dataset_exists(f, PATH_FACET_TOPO);
            bool haveFacetVals = dataset_exists(f, PATH_FACET_VALUES);
            if (haveFacetTopo && haveFacetVals) {
                std::vector<long long> facet_conn; hsize_t F=0;
                std::vector<long long> fVals;
                read_facet_topology(f, PATH_FACET_TOPO, N, facet_conn, F,
                                    info.nodes_per_facet, info.facet_index_base);
                info.num_facets = F;
                const hsize_t expected_facet_width = info.nodes_per_elem == 10 ? 6 : 3;
                if (info.nodes_per_facet != expected_facet_width)
                    throw std::runtime_error("Volume/facet order mismatch: Tet4 requires Tri3 and Tet10 requires Tri6 in " + path);
                if (mesh_nodes_per_facet == 0) mesh_nodes_per_facet = info.nodes_per_facet;
                if (info.nodes_per_facet != mesh_nodes_per_facet)
                    throw std::runtime_error("All facet topology datasets must have the same width");
                read_facet_values(f, PATH_FACET_VALUES, F, fVals);
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

                if (info.facet_index_base==1) for (auto& v: facet_conn) v -= 1;
                for (auto& v: facet_conn) v += node_offset;

                all_facet_conn.insert(all_facet_conn.end(), facet_conn.begin(), facet_conn.end());
                all_facet_vals.insert(all_facet_vals.end(), fVals.begin(), fVals.end());
                totalF += info.num_facets;
            } else if (!haveFacetTopo && !haveFacetVals) {
                info.num_facets = 0;
            } else {
                std::cerr << "Warning: facet datasets partially present in " << path
                          << " — skipping facets for this file.\n";
                info.num_facets = 0;
            }

            // Accumulate nodes & offsets
            all_nodes_xyz.insert(all_nodes_xyz.end(), nodes_xyz.begin(), nodes_xyz.end());
			for(size_t ni=0; ni < static_cast<size_t>(N); ++ni){
				all_node_groups.push_back(inp.group);
				all_node_file_ids.push_back(file_id);
			}
            node_offset += static_cast<long long>(N);
            totalN += N; totalM += M;
            file_infos.push_back(info);

            std::cout << "Read " << path
                      << "\n ? nodes="<<info.num_nodes
                      << " elems="<<info.num_elems
                      << " facets="<<info.num_facets
                      << "\n";
        file_id++;
		}

        if (totalN==0) { std::cerr << "No nodes read from inputs.\n"; return 2; }
        if (mesh_nodes_per_facet == 0)
            mesh_nodes_per_facet = mesh_nodes_per_elem == 10 ? 6 : 3;

        // Merge nodes ? map old?new
        NodeMerger merger(epsilon);
        std::vector<size_t> oldToNew(totalN);
        for (hsize_t i=0;i<totalN;++i) 
        {
            oldToNew[i] = merger.upsert(all_nodes_xyz[3*i+0],
                                        all_nodes_xyz[3*i+1],
                                        all_nodes_xyz[3*i+2],
										all_node_groups[i],
										all_node_file_ids[i]);
        }

        // Remap connectivity to merged node ids
        for (size_t k=0;k<all_elem_conn.size();++k)
        {
            long long old = all_elem_conn[k];
            if (old<0 || old>=static_cast<long long>(totalN))
                throw std::runtime_error("element node index OOB during remap");
            all_elem_conn[k] = static_cast<long long>(oldToNew[static_cast<size_t>(old)]);
        }
        for (size_t k=0;k<all_facet_conn.size();++k)
        {
            long long old = all_facet_conn[k];
            if (old<0 || old>=static_cast<long long>(totalN))
                throw std::runtime_error("facet node index OOB during remap");
            all_facet_conn[k] = static_cast<long long>(oldToNew[static_cast<size_t>(old)]);
        }

        if (!all_facet_conn.empty())
        {
        struct FaceKeyHash
        {
            size_t operator()(const std::array<long long,6>& a) const noexcept
            {
                size_t h = 0;
                for (long long v : a)
                    h ^= std::hash<long long>()(v + 0x9e3779b97f4a7c15ULL + (h<<6) + (h>>2));
                return h;
            }
        };
        auto canon = [&](size_t face_index)
        {
            const size_t width = static_cast<size_t>(mesh_nodes_per_facet);
            std::array<long long,6> t{{-1,-1,-1,-1,-1,-1}};
            for (size_t j=0; j<width; ++j)
                t[j] = all_facet_conn[face_index * width + j];
            std::sort(t.begin(), t.begin() + width);
            return t;
        };

        const size_t facet_width = static_cast<size_t>(mesh_nodes_per_facet);
        const size_t Fraw = all_facet_conn.size()/facet_width;
        std::unordered_map<std::array<long long,6>, int, FaceKeyHash> counts;
        counts.reserve(Fraw * 2);

        // count
        for (size_t fi=0; fi<Fraw; ++fi) 
        {
            auto key = canon(fi);
            counts[key] += 1;
        }

        // keep only unique faces
        std::vector<long long> kept_conn; kept_conn.reserve(all_facet_conn.size());
        std::vector<long long> kept_vals; kept_vals.reserve(all_facet_vals.size());
        size_t removed = 0;

        for (size_t fi=0; fi<Fraw; ++fi) 
        {
            auto key = canon(fi);
            if (counts[key] == 1) 
            {
                const auto begin = all_facet_conn.begin() + fi * facet_width;
                kept_conn.insert(kept_conn.end(), begin, begin + facet_width);
                kept_vals.push_back(all_facet_vals[fi]);
            } else 
            {
                ++removed; 
            }
        }

        std::cerr << removed
                    << " facets removed on internal faces (shared across parts)\n";
        all_facet_conn.swap(kept_conn);
        all_facet_vals.swap(kept_vals);
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
            all_elem_conn,
            all_elem_vals,
            all_facet_conn,
            all_facet_vals,
            N_unique,
            M_total,
            F_total,
            mesh_nodes_per_elem,
            mesh_nodes_per_facet,
            file_infos,
            epsilon);

        if (merge_fpd_enabled)
        {
            std::cerr << "Writing merged FPD to: " << merged_fpd_path << "\n" << std::flush;
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

    } catch (const FileIException& e) { std::cerr << "HDF5 file error: " << e.getDetailMsg() << "\n"; return 11;
    } catch (const DataSetIException& e) { std::cerr << "HDF5 dataset error: " << e.getDetailMsg() << "\n"; return 12;
    } catch (const DataSpaceIException& e) { std::cerr << "HDF5 dataspace error: " << e.getDetailMsg() << "\n"; return 13;
    } catch (const DataTypeIException& e) { std::cerr << "HDF5 datatype error: " << e.getDetailMsg() << "\n"; return 14;
    } catch (const std::exception& e) { std::cerr << "Error: " << e.what() << "\n"; return 15; }
}
