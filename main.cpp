#include <iostream>
#include <vector>
#include <string>

namespace BTAP {

    // A single Intermediate Representation (IR) instruction
    struct Instruction {
        std::string op;
        std::string dest;
        std::string s1;
        std::string s2;
        bool isGuard = false;
    };

    class Engine {
    private:
        std::vector<Instruction> irStream;

    public:
        // Lowering
        void emit(std::string op, std::string d, std::string s1, std::string s2 = "") {
            irStream.push_back({op, d, s1, s2, false});
        }
        // Analyzes the IR to inject security primitives
        void runProtectionPass() {
            std::cout << "[BTAP] Running Security Instrumentation Pass..." << std::endl;
            std::vector<Instruction> hardenedStream;
            
            for (const auto& i : irStream) {
                // inject a memory integrity guard for every global store
                if (i.op == "st.global") {
                    hardenedStream.push_back({"call", "verify_mem_ptr", i.dest, "", true});
                }
                hardenedStream.push_back(i);
            }
            irStream = hardenedStream;
        }

        void printPTX() {
            std::cout << "\n BTAP Output" << std::endl;
            std::cout << ".version 7.0\n.target sm_80\n\n";
            std::cout << ".visible .entry main_kernel() {\n";
            
            for (const auto& i : irStream) {
                if (i.isGuard) {
                    std::cout << "  BTAP Protection Guard" << std::endl;
                }
                
                std::cout << "  " << i.op << ".s32 " << i.dest << ", " << i.s1;
                if (!i.s2.empty()) {
                    std::cout << ", " << i.s2;
                }
                std::cout << ";" << std::endl;
            }
            
            std::cout << "  ret;\n}" << std::endl;
        }
    };
}

int main() {
    BTAP::Engine backend;

    backend.emit("add", "%r1", "%r2", "%r3");
    backend.emit("st.global", "[%ptr]", "%r1");

    backend.runProtectionPass();
    backend.printPTX();

    return 0;
}