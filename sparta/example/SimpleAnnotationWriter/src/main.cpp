#include "SimpleAnnotationOutputter.hpp"

int main() {
    SimpleAnnotationOutputter outputter("test_", 300); // 300 cycles per interval
    const auto clk_id = outputter.addClock(1, "core_clk");
    const auto loc_id = outputter.addLocation(clk_id, "top.test_location");
    outputter.startAnnotations();
    for(int i = 0; i < 1000; ++i) {
        outputter.writeAnnotation(loc_id, "uid=" + std::to_string(i)); // single cycle transactions
        outputter.tick(); // call tick at the end of every clock cycle
    }
    for(int i = 1000; i < 2000; ++i) {
        if(i % 2 == 0) {
            outputter.writeAnnotation(loc_id, "uid=" + std::to_string(i), 2); // 2 cycle long transactions
        }
        outputter.tick(); // call tick at the end of every clock cycle
    }
    return 0;
}
