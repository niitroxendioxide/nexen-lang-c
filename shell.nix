{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  nativeBuildInputs = with pkgs; [
    gcc
    gnumake
    lsd

    cmake
    ninja
    
    llvmPackages.clang-unwrapped # clangd binary
  ];

  shellHook = ''
    echo "Environment loaded"

    project_run() {
      if [ ! -d "build" ]; then
        echo "'build' folder not found. Creating project"
        cmake -S . -B build
        ln -sf build/compile_commands.json .
      fi

      if cmake --build build; then
        EXEC=$(find build -maxdepth 1 -type f -executable ! -name "CMakeCache.txt" | head -n 1)
        
        if [ -n "$EXEC" ]; then
          echo ""
          
          "$EXEC" ./input/main.nx
        else
          echo "[Error]: Build succeeded, but no executable file was found in ./build/"
        fi
      else
        echo "Build failed. Fix compiler errors."
      fi
    }
  '';
}

