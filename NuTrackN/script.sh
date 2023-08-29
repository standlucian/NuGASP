#!/bin/bash

# Start the socket server in the background and redirect input/output to a named pipe
mkfifo mypipe
trap "rm -f mypipe" EXIT
nc -l -p 12345 < mypipe | while read input; do
    echo "Received: $input"
    # You can process the input here as needed
done > mypipe &

# Send a message to the terminal
echo "Message from script" > mypipe

# Launch your Qt application
./canvas

