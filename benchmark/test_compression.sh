#!/bin/bash

echo "Checking server connection..."
curl -v http://localhost:7877/ 2>&1 | grep "Connected to"

echo -e "\nTrying with IP address instead of localhost..."
curl -v http://127.0.0.1:7877/ 2>&1 | grep "Connected to"

echo -e "\nTesting with IP address and without compression..."
curl -s -o /dev/null -w "HTTP Status: %{http_code}\nSize: %{size_download} bytes\nTime: %{time_total} seconds\n" http://127.0.0.1:7877/

echo -e "\nTesting with IP address and gzip compression..."
curl -s -H "Accept-Encoding: gzip" -o /dev/null -w "HTTP Status: %{http_code}\nSize: %{size_download} bytes\nTime: %{time_total} seconds\n" http://127.0.0.1:7877/

echo -e "\nVerbose output with compression headers (may show binary content):"
curl -s -i -H "Accept-Encoding: gzip" http://127.0.0.1:7877/ | grep -a -E "Content-|Vary|Encoding"

echo -e "\nComparing sizes and checking compression ratio:"
normal_size=$(curl -s http://127.0.0.1:7877/ | wc -c)
compressed_size=$(curl -s -H "Accept-Encoding: gzip" http://127.0.0.1:7877/ | wc -c)

echo "Normal size: $normal_size bytes"
echo "Compressed size: $compressed_size bytes"

if [ $compressed_size -lt $normal_size ] && [ $compressed_size -gt 0 ]; then
  saved_bytes=$((normal_size - compressed_size))
  percent_saved=$((saved_bytes * 100 / normal_size))
  echo "Compression working! Reduction: $percent_saved% ($saved_bytes bytes saved)"
else
  echo "Compression not effective. Same or larger size."
fi 