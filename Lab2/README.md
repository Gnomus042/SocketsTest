# Process Communication Benchmarks

| Communication Type    | Latency  | Throughput | Capacity |
| --------------------- | -------: | ---------: | -------: |
| Mmap (MAP_ANONYMOUS)  | 0.005ms  | 209 MB/s   | 240 MB/s |
| Shared memory (shmget)| 0.8ms    | 154 MB/s   | 157 MB/s |
| Files (read write)    | 0.1ms    | 188 MB/s   | 230 MB/s |
| Sockets (UNIX)        | 0.2ms    | 165 MB/s   | 180 MB/s |
| Sockets (INET)        | 0.9ms    |  23 MB/s   |  50 MB/s |

Sockets benchmarks were taken from previous labka