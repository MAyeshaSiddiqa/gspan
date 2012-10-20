
#include "gspan.hpp"
#include "graph_ops.hpp"

#include <stdlib.h>
#include <cstring>
#include <iostream>
#include <string>
#include <set>
#include <boost/graph/graphviz.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

#include <unistd.h> // getopt_long()

using namespace graph_alg;

typedef GraphOpsDefault<std::string, std::string, boost::directedS >      GraphOpsDir;
typedef GraphOpsDefault<std::string, std::string, boost::bidirectionalS > GraphOpsBidir;

bool verbose = false;
bool print_dfscode = false;

// ------------------ READ stdin ---------------------------------------
template<class GraphOps>
std::istream& contruct_dfsc(typename GraphOps::DFSCode& dfsc, std::string& tr_name, std::istream& is)
{
    typedef typename GraphOps::EdgeCode EdgeCode;
    typedef typename GraphOps::DFSCode DFSCode;
    typedef typename GraphOps::vertex_label_t VL;
    typedef typename GraphOps::edge_label_t   EL;
    typedef typename GraphOps::vertex_index_t VI;

    std::map<VI, VL> vlabels;

    char line[1024];
    while (true)
    {
        std::streampos pos = is.tellg();
        if (! is.getline(line, 1024))
            break;

        std::vector<std::string> result;       
        char* p = strtok(line, " \t");
        while (p)
        {
            result.push_back(std::string(p));
            p = strtok(0, " \t");
        }
        
        if (result.empty())
            continue;

        if (result[0] == "t")
        {
            if (!dfsc.empty())
            {
                is.seekg(pos, std::ios_base::beg);
                return is;
            }
            
            tr_name = result[2];
        }
        else if (result[0] == "v")
        {
            assert(result.size() == 3);
            vlabels[atoi(result[1].c_str())] = result[2];
        }
        else if (result[0] == "e")
        {
            assert(result.size() == 4);
            VI from   = atoi(result[1].c_str());
            VI to     = atoi(result[2].c_str());
            EL elabel = result[3];
	    EdgeCode ec(from, to, vlabels[from], elabel, vlabels[to]);
            dfsc.push(ec);
        }
    }
    return is;
}


// ------------------ RESULT -------------------------------------------

template<class GraphOps>
class Result
{
public:
    typedef typename GraphOps::graph_t Graph;
    typedef typename GraphOps::EdgeCode EdgeCode;
    typedef typename GraphOps::DFSCode DFSCode;
    typedef typename Traits<GraphOps>::Projected Projected;
    typedef typename GraphOps::vertex_label_t VL;
    typedef typename GraphOps::edge_label_t   EL;
    typedef typename GraphOps::vertex_index_t VI;
    
    Result(std::ostream& ostr, std::map<const Graph*, std::string>& tr_names, const GraphOps& ops)
	: ostr(ostr), tr_names(tr_names), ops(ops), ngraph(0) {}

    void print_dfsc(const DFSCode& dfsc, const Projected& projected);
    void print_tgf(const DFSCode& dfsc, const Projected& projected);

    void operator() (const Projected& projected, const DFSCode& dfsc)
	{
	    if (print_dfscode)
		print_dfsc(dfsc, projected);
	    else
		print_tgf(dfsc, projected);
	}
private:
    std::ostream& ostr;
    std::map<const Graph*, std::string>& tr_names;
    const GraphOps& ops;
    int ngraph;
};

template<class GraphOps>
void Result<GraphOps>::print_dfsc(const DFSCode& dfsc, const Projected& projected)
{
    ostr << dfsc << std::endl;

    if (verbose)
    {
	BOOST_FOREACH(const typename Projected::value_type sbg, projected)
	    ostr << "\t" << sbg;
    }
}

template<class GraphOps>
void Result<GraphOps>::print_tgf(const DFSCode& dfsc, const Projected& projected)
{
    std::map<VI,VL> vlabels;
    
    BOOST_FOREACH(const EdgeCode& ec, dfsc)
    {
	if (!ops.void_vlabel(ec.vl_from))vlabels[ec.vi_from] = ec.vl_from;
	if (!ops.void_vlabel(ec.vl_to))  vlabels[ec.vi_to] = ec.vl_to;
    }
    
    ostr << "t # " << ++ngraph << std::endl;

    for (typename std::map<VI,VL>::const_iterator i = vlabels.begin(); i != vlabels.end(); ++i)
	ostr << "v " << i->first << " " << i->second << std::endl;

    BOOST_FOREACH(const EdgeCode& ec, dfsc)
	ostr << "e " << ec.vi_from << " " << ec.vi_to << " " << ec.el << std::endl;


    std::set<const Graph*> gg;
    BOOST_FOREACH(const typename Traits<GraphOps>::SBG& sbg, projected)
	gg.insert(sbg.get_graph());
    ostr << "#found_in: ";
    for (typename std::set<const Graph*>::const_iterator i = gg.begin(); i != gg.end(); ++i)
    {
	typename std::set<const Graph*>::const_iterator tmpi = i;
	ostr << tr_names[*i];
	if (++tmpi != gg.end())
	    ostr << ", ";
	else
	    ostr << std::endl;
    }
    ostr << std::endl;
}


// ---------------------------------------------------------------------

template<class GraphOps>
void run(std::istream& is, int minsup)
{
    typedef typename GraphOps::graph_t Graph;
    typedef typename GraphOps::EdgeCode EdgeCode;
    typedef typename GraphOps::DFSCode DFSCode;
    typedef typename GraphOps::vertex_label_t VL;
    typedef typename GraphOps::edge_label_t   EL;
    typedef typename GraphOps::vertex_index_t VI;

    GraphOps ops;
    boost::ptr_vector<Graph> gr;
    std::map<const Graph*, std::string> tr_names;
    Result<GraphOps> result(std::cout, tr_names, ops);
    unsigned int skipped = 0;

    while (true)
    {
	std::string tr_name;
	DFSCode dfsc;
	contruct_dfsc<GraphOps>(dfsc, tr_name, is);
	if (dfsc.empty())
	    break;
	try {
	    Graph* graph = ops.create_graph(dfsc);
	    gr.push_back(graph);
	    tr_names[graph] = tr_name;

	    if (verbose)
		std::cerr << "INFO:    Graph " << tr_name << " was created"
			  << " at address " << graph << std::endl;
	}
	catch (typename GraphOps::VertexNotLabeledException e)
	{
	    ++skipped;
	    if (verbose)
		std::cerr << "WARNING: Graph " << tr_name << " not created, vertex "
			  << e.vertex_index << " not labeled" << std::endl;
	}
    }

    if (verbose)
	std::cerr << "Transactional graphs: " << gr.size() << " created, "
		  << skipped << " skipped" << std::endl;

    gspan(gr.begin(), gr.end(), minsup, ops, result);
}

std::ostream& usage(std::ostream& ostr)
{
    return ostr << "Usage: gspan <minsup> [-dir] [-dfsc] -v" << std::endl << std::endl;
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
	usage(std::cerr);
	return 1;
    }

    int minsup = atoi(argv[1]);

    bool directed =
	(argc > 2 && std::string(argv[2]) == "-dir") ||
	(argc > 3 && std::string(argv[3]) == "-dir") ||
	(argc > 4 && std::string(argv[4]) == "-dir");

    print_dfscode =
	(argc > 2 && std::string(argv[2]) == "-dfsc") ||
	(argc > 3 && std::string(argv[3]) == "-dfsc") ||
	(argc > 4 && std::string(argv[4]) == "-dfsc");

    verbose =
	(argc > 2 && std::string(argv[2]) == "-v") ||
	(argc > 3 && std::string(argv[3]) == "-v") ||
	(argc > 4 && std::string(argv[4]) == "-v");


    if (directed)
    {
	std::cout << "#directed" << std::endl;
	run<GraphOpsDir>(std::cin, minsup);
    }
    else
    {
	std::cout << "#undirected" << std::endl;
	run<GraphOpsBidir>(std::cin, minsup);
    }
    return 0;
}