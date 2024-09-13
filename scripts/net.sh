#!/bin/sh

wget_or_curl=$( (command -v wget > /dev/null 2>&1 && echo "wget -qO-") || \
                (command -v curl > /dev/null 2>&1 && echo "curl -skL"))

if [ -z "$wget_or_curl" ]; then
  >&2 printf "%s\n" "Neither wget or curl is installed." \
	         "Install one of these tools to download NNUE files automatically."
  exit 1
fi

fetch_network() {
  _filename="pikafish.nnue"

  if [ -f "$_filename" ]; then
    echo "Exists $_filename, skipping download"
    return
  fi

  url="https://github.com/official-pikafish/Networks/releases/download/master-net/$_filename"
    echo "Downloading from $url ..."
    if $wget_or_curl "$url" > "$_filename"; then
      echo "Successfully downloaded $_filename"
    else
      # Download was not successful, return false.
      echo "Failed to download $_filename from $url"
      return 1
    fi
}

$call fetch_network