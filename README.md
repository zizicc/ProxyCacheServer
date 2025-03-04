# HTTP Caching Proxy

## Overview
This project implements an HTTP caching proxy server in C++. It intercepts HTTP requests, forwards them to origin servers, and caches responses when applicable. It supports `GET`, `POST`, and `CONNECT` methods, handles concurrent requests using multithreading, and logs all requests with unique IDs.

## Features
- **HTTP Methods**: Supports `GET`, `POST`, and `CONNECT`.
- **Caching**: Stores `200-OK` GET responses, handles expiration & re-validation.
- **Multithreading**: Efficient handling of concurrent requests.
- **Logging**: Tracks requests, cache status, server interactions, and errors.
- **Error Handling**: Graceful failure management for malformed requests & responses.
- **Dockerized Deployment**: Includes `Dockerfile` & `docker-compose.yml`.

## Setup
### Prerequisites
- C++17 or later, Docker, GNU Make

### Build & Run
```sh
git clone <repository-url>
cd <repository>
cd docker-deploy
sudo docker-compose up --build
```
The proxy listens on port `12345`, logs are stored in `logs/`.

## Usage
### Configure Browser
- Configure your browser to use a manual proxy configuration and input the hostname of the machine and port (12345) where the proxy is running. 

### Log Format
```plaintext
ID: "REQUEST" from IP @ TIME
ID: not in cache
ID: cached, expires at EXPIRES
ID: Requesting "REQUEST" from SERVER
ID: Received "RESPONSE" from SERVER
ID: Responding "RESPONSE"
ID: Tunnel closed
```

### Manual Tests
#### Send Malformed Request
```sh
echo -e "BADREQUEST / HTTP/1.1\r\n\r\n" | nc -q 1 127.0.0.1 12345
```
#### Fetch Webpage Using Proxy
```sh
wget -e use_proxy=yes -e http_proxy=http://127.0.0.1:12345 http://www.example.com
```

## Implementation
- **Multithreading**: Uses `std::thread`, synchronized cache with `std::mutex`.
- **Design**: RAII, exception handling, modular components.


## Future Improvements
- Support `PUT`, `DELETE`
- LRU-based cache eviction
- Thread pool optimization

## License
MIT License.
