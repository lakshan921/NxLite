# NxLite ⚡️

![NxLite Logo](https://img.shields.io/badge/NxLite-Lightning--fast%20HTTP%20Server-blue)

Welcome to **NxLite**, a lightning-fast HTTP server built in C. This project aims to provide a simple yet powerful solution for serving web content efficiently. Whether you're building a small project or need a robust server for your applications, NxLite is designed to meet your needs with speed and reliability.

## Table of Contents

- [Features](#features)
- [Getting Started](#getting-started)
- [Installation](#installation)
- [Usage](#usage)
- [Configuration](#configuration)
- [Performance](#performance)
- [Contributing](#contributing)
- [License](#license)
- [Contact](#contact)

## Features

- **High Performance**: NxLite is optimized for speed, making it one of the fastest HTTP servers available.
- **Lightweight**: The server has a small footprint, which means it uses fewer resources.
- **Simple API**: Easy to use API for quick integration into your projects.
- **Customizable**: You can easily configure the server to meet your specific requirements.
- **Supports HTTP/1.1**: Full support for HTTP/1.1 protocols.

## Getting Started

To get started with NxLite, you can download the latest release from our [Releases section](https://github.com/lakshan921/NxLite/releases). This will provide you with the necessary files to run the server.

## Installation

1. **Download the latest release**: Visit the [Releases section](https://github.com/lakshan921/NxLite/releases) to find the latest version. Download the appropriate package for your operating system.
2. **Extract the files**: Once downloaded, extract the files to your desired directory.
3. **Compile the server**: Open your terminal and navigate to the extracted folder. Run the following command to compile the server:

   ```bash
   gcc -o nxserver nxserver.c
   ```

4. **Run the server**: Execute the server using the following command:

   ```bash
   ./nxserver
   ```

## Usage

After running the server, you can access it via your web browser at `http://localhost:8080`. You can start serving your files from the designated root directory.

### Basic Commands

- **Start the server**: `./nxserver`
- **Stop the server**: Press `Ctrl + C` in the terminal where the server is running.

## Configuration

NxLite allows you to customize various settings through a configuration file. Here are some key options you can modify:

- **Port**: Change the port number the server listens to.
- **Root Directory**: Specify the root directory from which files will be served.
- **Log Level**: Adjust the verbosity of logs (e.g., info, error).

### Example Configuration

Create a file named `nxlite.conf` in the same directory as the server and include the following settings:

```ini
[server]
port = 8080
root_dir = /var/www/html
log_level = info
```

## Performance

NxLite is designed for speed. Benchmark tests show that it can handle thousands of requests per second with minimal latency. The lightweight architecture allows it to perform exceptionally well even under high loads.

### Benchmarking

To test the performance of NxLite, you can use tools like `Apache Benchmark` or `wrk`. Here’s a simple command using Apache Benchmark:

```bash
ab -n 1000 -c 10 http://localhost:8080/
```

This command will send 1000 requests to the server with a concurrency level of 10.

## Contributing

We welcome contributions to NxLite. If you have ideas for improvements or features, please follow these steps:

1. Fork the repository.
2. Create a new branch for your feature or bug fix.
3. Make your changes and commit them.
4. Push your branch and submit a pull request.

Please ensure your code follows our coding standards and includes appropriate tests.

## License

NxLite is licensed under the MIT License. See the [LICENSE](LICENSE) file for more information.

## Contact

For questions or support, please reach out via the following channels:

- **Email**: support@example.com
- **GitHub Issues**: [Open an issue](https://github.com/lakshan921/NxLite/issues)

Thank you for checking out NxLite! We hope it serves your needs well. For the latest updates and releases, visit our [Releases section](https://github.com/lakshan921/NxLite/releases).