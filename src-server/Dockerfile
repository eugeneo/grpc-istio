# Use the Alpine base image
FROM ubuntu

WORKDIR /app

# Expose port 4004
EXPOSE 4004

ENV GRPC_XDS_EXPERIMENTAL_ENABLE_RING_HASH=true
ENV GRPC_EXPERIMENTAL_XDS_ENABLE_OVERRIDE_HOST=true

RUN apt-get update && \
  apt-get install -y curl && \
  rm -rf /var/lib/apt/lists/*

COPY echo-server .

# Make the executable executable
RUN chmod +x /app/echo-server

# Set the default command to run when the container starts
ENTRYPOINT ["/app/echo-server"]

CMD [ "/app/echo-server" ]
