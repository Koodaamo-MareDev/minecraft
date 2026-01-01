FROM mcr.microsoft.com/devcontainers/base:ubuntu-24.04

WORKDIR /minecraft

COPY ./setup-env.sh .

RUN chmod +x setup-env.sh && ./setup-env.sh

ENV DEVKITPRO=/opt/devkitpro
ENV DEVKITPPC=/opt/devkitpro/devkitPPC
