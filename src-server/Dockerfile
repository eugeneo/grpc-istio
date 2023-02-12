# Use the Alpine base image
FROM ubuntu

WORKDIR /app

# Expose port 4004
EXPOSE 4004

# Copy the pre-built executable into the container
COPY echo_server .

# Make the executable executable
RUN chmod +x /app/echo_server

# Set the default command to run when the container starts
CMD ["/app/echo_server"]
