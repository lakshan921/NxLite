#!/bin/bash
   
   echo "Simulating first visit (cold cache)..."
   time curl -s http://localhost:7888/index.html > /dev/null
   
   echo "Simulating return visit (warm cache)..."
   ETAG=$(curl -s -I http://localhost:7888/index.html | grep ETag | awk '{print $2}' | tr -d '\r')
   time curl -s -H "If-None-Match: $ETAG" http://localhost:7888/index.html > /dev/null
   
   echo "Bandwidth comparison:"
   echo "First visit: $(curl -s http://localhost:7888/index.html | wc -c) bytes"
   echo "Return visit: $(curl -s -H "If-None-Match: $ETAG" -w '%{size_download}' http://localhost:7888/index.html -o /dev/null) bytes"