#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
// C++
#include <iostream>
#include <vector>
#include <cassert>
#include <chrono>
#include <fstream>

const uint64_t PACKET_SIZE = 1024 * 512;

typedef std::chrono::duration<double, std::ratio<1> > second_;

class Timer {
private:
    std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> prev_time;
public:
    Timer() {
        prev_time = std::chrono::high_resolution_clock::now();
    }

    double GetElapsedSeconds() {
        auto cur_time = std::chrono::high_resolution_clock::now();
        auto delta = std::chrono::duration_cast<second_>(cur_time - prev_time).count();
        prev_time = cur_time;
        return delta;
    }
};

struct IO {
    virtual void write_bytes(const std::vector<uint8_t> &bytes) = 0;

    virtual std::vector<uint8_t> read_bytes() = 0;

    virtual void close() = 0;

    void ComputeLatency() {
        uint64_t repeats = 1;
        double total_latency = 0;
        for (uint64_t k = 0; k < repeats; k++) {
            Timer timer;
            std::vector<uint8_t> data(16);
            for (uint64_t i = 0; i < data.size(); i++) {
                data[i] = i * 3 + 23;
            }
            write_bytes(data);
            auto response = read_bytes();
            for (uint64_t i = 0; i < data.size(); i++) {
                assert(data[i] == response[i]);
            }
            total_latency += timer.GetElapsedSeconds() / 2;
        }

        std::cout << "Latency: " << total_latency / repeats << " s" << std::endl;
    }

    void ComputeThroughput() {
        Timer timer;
        std::vector<uint8_t> data(PACKET_SIZE);
        for (uint64_t i = 0; i < data.size(); i++) {
            data[i] = i * 3 + 23;
        }
        uint64_t mega_bytes = 128;
        for (uint64_t k = 0; k < mega_bytes * 1024 * 1024 / PACKET_SIZE; k++) {
            write_bytes(data);
            auto response = read_bytes();
            for (uint64_t i = 0; i < data.size(); i++) {
                assert(data[i] == response[i]);
            }
        }
        double time = timer.GetElapsedSeconds();
        double throughput = mega_bytes / time * 2;
        std::cout << "Throughput: " << throughput << " MB/s" << std::endl;
    }

    void ComputeCapacity() {
        double max_throughput = 0;
        for(int kk = 0; kk < 16; kk++) {
            Timer timer;
            std::vector<uint8_t> data(PACKET_SIZE);
            for (uint64_t i = 0; i < data.size(); i++) {
                data[i] = i * 3 + 23;
            }
            uint64_t mega_bytes = 16;
            for (uint64_t k = 0; k < mega_bytes * 1024 * 1024 / PACKET_SIZE; k++) {
                write_bytes(data);
                auto response = read_bytes();
                for (uint64_t i = 0; i < data.size(); i++) {
                    assert(data[i] == response[i]);
                }
            }
            double time = timer.GetElapsedSeconds();
            double throughput = mega_bytes / time * 2;
            max_throughput = std::max(max_throughput, throughput);
        }
        std::cout << "Capacity: " << max_throughput << " MB/s" << std::endl;
    }
};

struct FileIO : public IO {
    FileIO(const std::string &filename, int sender) : sender(sender) {
        file.open(filename, std::fstream::in | std::fstream::out);
        int temp = 0;
        file.seekp(0);
        file.write(reinterpret_cast<const char *>(&sender), sizeof(int));
        file.write(reinterpret_cast<const char *>(&temp), sizeof(int));
        file.flush();
    }

    void write_bytes(const std::vector<uint8_t> &bytes) override {
        if (closed) {
            return;
        }
        int other = 0;
        int prev = 0;
        do {
            file.seekg(0);
            file.read(reinterpret_cast<char *>(&other), sizeof(int));
            file.read(reinterpret_cast<char *>(&prev), sizeof(int));
            if (prev == -1) {
                closed = true;
                return;
            }
        } while (prev != 0);

        file.seekp(2 * sizeof(int));
        file.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        int size = bytes.size();
        file.seekp(0);
        file.write(reinterpret_cast<const char *>(&sender), sizeof(int));
        file.write(reinterpret_cast<const char *>(&size), sizeof(int));
        file.flush();
        //std::cout << "[" << sender << "] Data sent" << std::endl;
    }

    void close() override {
        closed = true;
        int temp = -1;
        file.seekp(0);
        file.write(reinterpret_cast<const char *>(&sender), sizeof(int));
        file.write(reinterpret_cast<const char *>(&temp), sizeof(int));
        file.flush();
    }

    std::vector<uint8_t> read_bytes() override {
        if (closed) {
            return std::vector<uint8_t>();;
        }
        int size = 0;
        int other = 0;
        while (!size || other == sender) {
            file.flush();
            file.seekg(0);
            file.read(reinterpret_cast<char *>(&other), sizeof(int));
            file.read(reinterpret_cast<char *>(&size), sizeof(int));
        }
        if (size == -1) {
            closed = true;
            return std::vector<uint8_t>();
        }
        std::vector<uint8_t> data(size);
        file.seekg(sizeof(int) * 2);
        file.read(reinterpret_cast<char *>(data.data()), size);

        int temp = 0;
        file.seekp(0);
        file.write(reinterpret_cast<const char *>(&sender), sizeof(int));
        file.write(reinterpret_cast<const char *>(&temp), sizeof(int));
        file.flush();

        return data;
    }

    std::fstream file;
    bool closed = false;
    int sender = 0;
};

struct MmapIO : public IO {
    MmapIO(uint8_t *ptr, int sender) : data_ptr(ptr + sizeof(int) * 2), sender(sender) {
        auto int_ptr = reinterpret_cast<int *>(ptr);
        other_ptr = int_ptr;
        size_ptr = int_ptr + 1;
    }

    void close() override {
        closed = true;
        *size_ptr = -1;
    }

    void write_bytes(const std::vector<uint8_t> &bytes) override {
        if (closed) {
            return;
        }
        while (*size_ptr) {}
        if (*size_ptr == -1) {
            close();
            return;
        }
        std::copy(bytes.begin(), bytes.end(), data_ptr);
        *size_ptr = bytes.size();
        *other_ptr = sender;
    }

    std::vector<uint8_t> read_bytes() override {
        if (closed) {
            return std::vector<uint8_t>();
        }
        int size, other;
        do {
            other = *other_ptr;
            size = *size_ptr;
        } while (!size || other == sender);
        if (size == -1) {
            close();
            return std::vector<uint8_t>();
        }
        std::vector<uint8_t> data(size);
        std::copy(data_ptr, data_ptr + size, data.data());
        *size_ptr = 0;
        *other_ptr = sender;
        return data;
    }

    int sender;
    int *other_ptr;
    int *size_ptr;
    uint8_t *data_ptr;
    bool closed = false;
};


struct shm_t{
    int id;
    size_t size;
};

shm_t *shm_new(size_t size) {
    shm_t *shm = new shm_t();
    shm->size = size;

    if ((shm->id = shmget(IPC_PRIVATE, size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR)) < 0) {
        perror("shmget");
        free(shm);
        return NULL;
    }

    return shm;
}

void shm_write(shm_t *shm, char *data, int offset, int size) {
    void *shm_data;

    if ((shm_data = shmat(shm->id, NULL, 0)) == (void *) -1) {
        perror("write");
        return;
    }

    memcpy((char *) shm_data + offset, data, size);
    shmdt(shm_data);
}

void shm_read(char *data, shm_t *shm, int offset, int size) {
    void *shm_data;

    if ((shm_data = shmat(shm->id, NULL, 0)) == (void *) -1) {
        perror("read");
        return;
    }
    memcpy(data, (char *) shm_data + offset, size);
    shmdt(shm_data);
}

void shm_del(shm_t *shm) {
    shmctl(shm->id, IPC_RMID, 0);
    free(shm);
}

struct SharedIO : public IO {
    SharedIO(shm_t *shm, int sender) : shm(shm), sender(sender) {}

    shm_t *shm;

    void close() override {
        closed = true;
        int temp = -1;
        shm_write(shm, (char *) &temp, sizeof(int), sizeof(int));
    }

    void write_bytes(const std::vector<uint8_t> &bytes) override {
        if (closed) {
            return;
        }
        int size = 0;
        while (size) {
            shm_read((char *) &size, shm, sizeof(int), sizeof(int));
            if (size == -1) {
                close();
                return;
            }
        }
        shm_write(shm, (char *) bytes.data(), sizeof(int) * 2, bytes.size());
        int temp = bytes.size();
        shm_write(shm, (char *) &temp, sizeof(int), sizeof(int));
        shm_write(shm, (char *) &sender, 0, sizeof(int));
    }

    std::vector<uint8_t> read_bytes() override {
        if (closed) {
            return std::vector<uint8_t>();
        }
        int size, other;
        do {
            shm_read((char *) &other, shm, 0, sizeof(int));
            shm_read((char *) &size, shm, sizeof(int), sizeof(int));
        } while (!size || other == sender);
        if (size == -1) {
            close();
            return std::vector<uint8_t>();
        }
        std::vector<uint8_t> data(size);
        shm_read((char *) data.data(), shm, sizeof(int) * 2, data.size());

        int temp = 0;
        shm_write(shm, (char *) &temp, sizeof(int), sizeof(int));
        shm_write(shm, (char *) &sender, 0, sizeof(int));
        return data;
    }

    int sender;
    bool closed = false;
};

void RunBenchmark(const std::string &name, IO *io, IO *io_other) {
    std::cout << "Benchmark for method \"" << name << "\" started" << std::endl;
    int p = fork();
    if (p == 0) {
        // child
        std::vector<uint8_t> data;
        do {
            data = io_other->read_bytes();
            if (!data.empty()) {
                io_other->write_bytes(data);
            }
        } while (!data.empty());
        exit(0);
    } else {
        // parent
        io->ComputeLatency();
        io->ComputeThroughput();
        io->ComputeCapacity();
        io->close();
    }
}

int main() {
    // preparing
    std::ofstream f("file.data");
    f.close();
    uint8_t *ptr = reinterpret_cast<uint8_t *>(mmap(NULL, (PACKET_SIZE + 8) * sizeof(uint8_t),
                                                    PROT_READ | PROT_WRITE,
                                                    MAP_SHARED | MAP_ANONYMOUS,
                                                    0, 0));

    shm_t * shm = shm_new((PACKET_SIZE + 8) * sizeof(uint8_t));

    RunBenchmark("Files", new FileIO("file.data", 0), new FileIO("file.data", 1));
    RunBenchmark("Mmap", new MmapIO(ptr, 0), new MmapIO(ptr, 1));
    RunBenchmark("Mmap", new SharedIO(shm, 0), new SharedIO(shm, 1));
    return 0;
}
