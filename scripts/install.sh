#!/usr/bin/env sh
set -eu

repo="${ARX_PISTORIS_REPO:-Merxtef/arx-pistoris}"
version="${ARX_PISTORIS_VERSION:-latest}"
install_dir="${ARX_PISTORIS_INSTALL_DIR:-$HOME/.local/bin}"

os="$(uname -s)"
arch="$(uname -m)"

case "$os" in
  Linux) platform="linux" ;;
  *) echo "error: unsupported OS: $os" >&2; exit 1 ;;
esac

case "$arch" in
  x86_64|amd64) arch="x64" ;;
  *) echo "error: unsupported architecture: $arch" >&2; exit 1 ;;
esac

asset="arx-pistoris-$platform-$arch.tar.gz"
tmp="${TMPDIR:-/tmp}/arx-pistoris-install.$$"
mkdir -p "$tmp"
trap 'rm -rf "$tmp"' EXIT INT TERM

if [ "$version" = "latest" ]; then
  api_url="https://api.github.com/repos/$repo/releases/latest"
else
  api_url="https://api.github.com/repos/$repo/releases/tags/$version"
fi

download_url="$(curl -fsSL "$api_url" |
  sed -n 's/.*"browser_download_url":[[:space:]]*"\([^"]*'"$asset"'\)".*/\1/p' |
  head -n 1)"

if [ -z "$download_url" ]; then
  echo "error: release asset not found: $asset in $repo ($version)" >&2
  exit 1
fi

curl -fL "$download_url" -o "$tmp/$asset"
tar -xzf "$tmp/$asset" -C "$tmp"

exe="$(find "$tmp" -type f -name arx-pistor -perm -u+x | head -n 1)"
if [ -z "$exe" ]; then
  exe="$(find "$tmp" -type f -name arx-pistor | head -n 1)"
fi
if [ -z "$exe" ]; then
  echo "error: arx-pistor not found in $asset" >&2
  exit 1
fi

mkdir -p "$install_dir"
cp "$exe" "$install_dir/arx-pistor"
chmod 755 "$install_dir/arx-pistor"

case ":$PATH:" in
  *":$install_dir:"*) ;;
  *)
    path_line="export PATH=\"$install_dir:\$PATH\""
    updated=""

    add_path_line() {
      file="$1"
      touch "$file"
      if ! grep -F "$path_line" "$file" >/dev/null 2>&1; then
        printf '\n%s\n' "$path_line" >> "$file"
      fi
      updated="${updated}${updated:+, }$file"
    }

    if [ "$install_dir" = "$HOME/.local/bin" ]; then
      add_path_line "$HOME/.profile"
    fi

    case "${SHELL:-}" in
      */bash) add_path_line "$HOME/.bashrc" ;;
      */zsh) add_path_line "$HOME/.zshrc" ;;
    esac

    if [ -n "$updated" ]; then
      echo "Added $install_dir to PATH in $updated"
    else
      echo "Add this to your shell profile:"
      echo "  $path_line"
    fi
    echo "Open a new terminal before running arx-pistor by name."
    ;;
esac

echo "Installed: $install_dir/arx-pistor"
