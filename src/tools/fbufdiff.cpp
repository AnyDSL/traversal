#include <iostream>
#include <fstream>
#include <cmath>

int main(int argc, char**argv) {
    if (argc != 4) {
        std::cout << "usage: fbufdiff a.fbuf b.fbuf output.fbuf" << std::endl;
        return 0;
    }

    std::ifstream a(argv[1], std::ifstream::binary);
    std::ifstream b(argv[2], std::ifstream::binary);
    std::ofstream c(argv[3], std::ofstream::binary);

    if (!a || !b || !c) {
        std::cerr << "Cannot open file(s)." << std::endl;
        return 1;
    }

    a.seekg(0, std::ifstream::end);
    size_t n = a.tellg() / sizeof(float);
    a.seekg(0);

    int k = 0;
    float m = 0;
    for (int i = 0; i < n; i++) {
        float fa, fb, fc;
        a.read((char*)&fa, sizeof(float));
        b.read((char*)&fb, sizeof(float));
        fc = fabsf(fa - fb);
        k += fc != 0;
        m += fc;
        c.write((char*)&fc, sizeof(float));
    }

    std::cout << k << " difference(s), average error is " << m / k << std::endl;

    return 0;
}
