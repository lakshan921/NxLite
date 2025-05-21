# NxLite

A high-performance HTTP server written in C, designed for maximum efficiency and concurrency.

## Features

- **High Performance**: Optimized for handling thousands of concurrent connections
- **Zero-Copy**: Uses sendfile() for efficient file transfers
- **Memory Pooling**: Custom memory allocation for reduced fragmentation
- **Non-Blocking I/O**: Event-driven architecture using epoll
- **Master-Worker Architecture**: Pre-fork model similar to Nginx
- **Response Caching**: In-memory caching for static assets
- **Keep-Alive Support**: Persistent connections for reduced latency

## Architecture

### Master-Worker Model

NxLite uses a pre-fork model with a master process that manages multiple worker processes:

1. **Master Process**: 
   - Reads configuration
   - Creates listening socket
   - Forks worker processes
   - Monitors workers and respawns them if needed
   - Handles graceful shutdown

2. **Worker Processes**:
   - Accept connections from the shared listening socket
   - Process HTTP requests
   - Serve static files
   - Handle keep-alive connections

### Server Architecture

```
┌─────────────┐
│  Master     │
│  Process    │
└─────┬───────┘
      │
      ├─────────┬─────────┬─────────┐
      │         │         │         │
┌─────▼───┐ ┌───▼─────┐ ┌─▼───────┐ ┌▼────────┐
│ Worker 1 │ │ Worker 2 │ │ Worker 3│ │ Worker n│
└─────┬───┘ └───┬─────┘ └─┬───────┘ └┬────────┘
      │         │         │          │
      ▼         ▼         ▼          ▼
┌─────────────────────────────────────────────┐
│               Shared Socket                 │
└─────────────────────────────────────────────┘
```

### HTTP Request Flow

1. Client connects to server
2. Worker accepts connection
3. Worker reads and parses HTTP request
4. Request is processed:
   - Static file is served (with zero-copy if possible)
   - Response is cached for future requests
5. Connection is either closed or kept alive for future requests

## Building and Running

### Prerequisites

- GCC or Clang
- CMake (3.10+)
- Linux kernel 2.6+ (for epoll support)

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/dexter-xd/NxLite.git
cd NxLite

# Create build directory
mkdir build
cd build

# Build
cmake ..
make
```

### Running the Server

```bash
# Run with default settings
./nxlite

# Run with custom configuration
./nxlite -c /path/to/config/server.conf
```

## Configuration

NxLite can be configured through a configuration file:

```
# Example configuration
port=7888
worker_processes=8
root=../static
log=./logs/access.log
max_connections=100000
keep_alive_timeout=120 
```

## Logs

### Viewing Logs

```bash
# View access log in real-time
tail -f ./build/access.log
```

## Benchmarking

NxLite is designed for high performance. Here are some tools to benchmark it:

### Apache Bench (ab)

```bash
# Test with 1000 requests, 100 concurrent connections
ab -n 1000 -c 100 http://localhost:7888/

# Test with keep-alive connections
ab -n 10000 -c 100 -k http://localhost:7888/
```

### wrk

```bash
# Run a 30-second test with 12 threads and 400 connections
wrk -t12 -c400 -d30s http://localhost:7888/

# Run with a custom script
wrk -t4 -c100 -d30s -s script.lua http://localhost:7888/
```

### hey

```bash
# Send 10000 requests with 100 concurrent workers
hey -n 10000 -c 100 http://localhost:7888/

# Send requests for 10 seconds with 50 concurrent workers
hey -z 10s -c 50 http://localhost:7888/
```

## Performance Tuning

For optimal performance:

1. Increase system limits in `/etc/sysctl.conf`:
   ```
   fs.file-max = 100000
   net.core.somaxconn = 65536
   net.ipv4.tcp_max_syn_backlog = 65536
   ```

2. Adjust worker processes to match CPU cores:
   ```
   worker_processes auto;
   ```

3. Optimize TCP settings:
   ```
   net.ipv4.tcp_fin_timeout = 30
   net.ipv4.tcp_keepalive_time = 300
   ```

## License

This project is licensed under the MIT License - see the LICENSE file for details.
