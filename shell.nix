{
  pkgs ? import <nixpkgs> { },
}:

pkgs.mkShell {
  buildInputs = with pkgs; [
    go
    gotools
    gopls
    gpac # mp4box
  ];
}
