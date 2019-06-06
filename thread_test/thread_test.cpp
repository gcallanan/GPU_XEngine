#include <cstdio>
#include "tbb/flow_graph.h"

//using namespace tbb::flow;
typedef tbb::flow::multifunction_node<tbb::flow::continue_msg, tbb::flow::tuple<tbb::flow::continue_msg> > multi_node;

class body {
    public:
        body(const char *name) : my_name(name),temp(0) {}
        void operator()(tbb::flow::continue_msg){
            printf("%s %i\n", my_name.c_str(),temp++);            
        }
        void operator()(int i){
            printf("%s %i\n", my_name.c_str(),temp++);
            
        }
    private:
        std::string my_name;
        int temp;
};

class MultiBody {
    public:
        MultiBody(const char * name) : my_name(name),temp(0){};
        void operator()(tbb::flow::continue_msg, multi_node::output_ports_type &op) {
            temp++;
            if(temp%4==0){
                printf("%s %i\n", my_name.c_str(),temp);
                std::get<0>(op).try_put(1); // put to even queue
            }
        }
    private:
        int temp;
        std::string my_name;
};

int main() {
    tbb::flow::graph g;
    tbb::flow::broadcast_node<tbb::flow::continue_msg > start(g);
    tbb::flow::continue_node<tbb::flow::continue_msg> a(g, body("A"));
    tbb::flow::continue_node<tbb::flow::continue_msg> b(g, body("B"));
    tbb::flow::continue_node<tbb::flow::continue_msg> c(g, body("C"));
    multi_node d(g,tbb::flow::unlimited,MultiBody("D"));
    tbb::flow::continue_node<tbb::flow::continue_msg> e(g, body("E"));
    tbb::flow::continue_node<tbb::flow::continue_msg> f(g, body("F"));

    tbb::flow::make_edge(start, a);
    tbb::flow::make_edge(a, b);
    tbb::flow::make_edge(b, c);
    tbb::flow::make_edge(c, d);
    tbb::flow::make_edge(tbb::flow::output_port<0>(d), e);
    tbb::flow::make_edge(e, f);

    for (int i = 0; i < 12; ++i) {
        start.try_put(tbb::flow::continue_msg());
        g.wait_for_all();
    }

    return 0;
}