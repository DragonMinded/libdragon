# syntax=docker/dockerfile:1

# Just provide a variable for testing
FROM ubuntu:18.04
ARG N64_INST=/n64_toolchain
ENV N64_INST=${N64_INST}
