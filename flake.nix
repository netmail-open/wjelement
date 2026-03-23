{
	description = "Development flake";
	inputs = {
		nixpkgs.url	= "github:NixOS/nixpkgs/nixos-unstable";
		utils.url	= "github:numtide/flake-utils";
	};

	outputs = { nixpkgs, ... }@inputs: inputs.utils.lib.eachDefaultSystem (system:
		let
			pkgs = import nixpkgs {
				inherit system;
			};

			buildDeps = with pkgs; [
				cmakeCurses
				pkg-config
				ninja
				coreutils

				gdb
				lldb
			];
		in rec {
			# This block here is used when running `nix develop`
			devShells.default = pkgs.mkShell  {
				# Update the name to something that suites your project.
				name				= "wjelement";
				buildInputs			= buildDeps;
				packages			= buildDeps;
			};

			# Build with `nix build`
			packages.wjelement = pkgs.stdenv.mkDerivation {
				name				= "wjelement";
				buildInputs			= buildDeps;
				version				= "1.3";
				src					= ./.;

				cmakeFlags			= [ ];

				meta = {
					homepage		= "https://github.com/netmail-open/wjelement";
					description		= "JSON manipulation in C";
					license			= pkgs.lib.licenses.mit;
					platforms		= pkgs.lib.platforms.linux;
				};
			};
			packages.default		= packages.wjelement;

		}
	);
}
