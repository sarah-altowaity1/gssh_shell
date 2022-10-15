# Pull base image.
FROM ubuntu:latest

RUN apt-get -y update && apt-get install make 
RUN apt install -y build-essential
RUN apt-get install -y gdb

WORKDIR /root

RUN mkdir shell

COPY . ./shell