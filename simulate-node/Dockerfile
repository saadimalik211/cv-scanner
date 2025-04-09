FROM node:18-alpine

WORKDIR /app

# Copy package files and install dependencies
COPY package.json package-lock.json* ./
RUN npm install --production

# Copy application code
COPY server.js ./

# Create a volume for persistent data storage
VOLUME ["/app/data"]

# Expose the service port
EXPOSE 3000

# Run the service
CMD ["node", "server.js"] 