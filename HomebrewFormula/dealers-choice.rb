# frozen_string_literal: true

# The comment above, and the argument alignment in install, are here because this
# file lives in HomebrewFormula/ rather than Formula/ -- Homebrew's formula
# rubocop rules only apply to the latter, so the generic Ruby cops run and
# brew audit --strict fails without them. No formula in homebrew-core carries
# either; do not "tidy" them away.

class DealersChoice < Formula
  desc "Casual multiplayer online poker game"
  homepage "https://dealer-s-choice.github.io/"
  # The release asset, not the tag archive: meson dist bundles the subprojects.
  url "https://github.com/Dealer-s-Choice/dealers_choice/releases/download/v0.0.15/dealers-choice-0.0.15.tar.xz"
  sha256 "c8c7030d281c0e8f14faf7473478f366ad2f22391f05f9f7deaa33066df39b89"
  license "MIT"
  # No head spec yet: trunk needs canfigger >= 0.3.3 and homebrew-core has
  # 0.3.2, while std_meson_args blocks the wrap fallback. Add it back once
  # canfigger 0.3.3 is in homebrew-core.

  depends_on "meson" => :build
  depends_on "ninja" => :build
  depends_on "pkgconf" => :build
  depends_on "canfigger"
  depends_on "gettext"
  depends_on "libsodium"
  depends_on "protobuf-c"
  depends_on "sdl2"
  depends_on "sdl2_image"
  depends_on "sdl2_ttf"

  # std_meson_args passes --wrap-mode=nofallback, which stops the
  # dependency('miniaudio', fallback: ...) lookup from using the bundled copy.
  # miniaudio is meant to be vendored (upstream gives no ABI guarantee), and
  # meson.build pins its exact version, which homebrew-core's miniaudio formula
  # will not track. So force the fallback for it alone.
  def install
    system "meson", "setup", "build", *std_meson_args,
           "--force-fallback-for=miniaudio"
    system "meson", "compile", "-C", "build", "--verbose"
    system "meson", "install", "-C", "build"
  end

  # The client is a GUI program; --version is handled before SDL starts, so it
  # is the one thing that runs on a headless runner.
  test do
    assert_path_exists bin/"dealers-choice-bot"
    output = shell_output("#{bin}/dealers-choice --version")
    assert_match "Dealer's Choice", output
    assert_match version.to_s, output
  end
end
