#!/bin/bash
cd "$(dirname "$0")"
cmake -B build && cmake --build build && ./build/yiliya_game
