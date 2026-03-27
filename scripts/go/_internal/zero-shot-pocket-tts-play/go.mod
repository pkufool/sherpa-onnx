module zero-shot-pocket-tts-play

go 1.24.0

toolchain go1.24.13

replace github.com/k2-fsa/sherpa-onnx-go/sherpa_onnx => ../

require (
	github.com/ebitengine/oto/v3 v3.4.0
	github.com/k2-fsa/sherpa-onnx-go/sherpa_onnx v0.0.0-00010101000000-000000000000
	github.com/spf13/pflag v1.0.10
)

require (
	github.com/ebitengine/purego v0.9.0 // indirect
	golang.org/x/sys v0.36.0 // indirect
)
