# Variables
SRC = sparta_book.adoc
BUILD_DIR = build
HTML_OUT_DIR = $(BUILD_DIR)/html
PDF_OUT = $(BUILD_DIR)/book.pdf

# Default target
all: html pdf

# Create build directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(HTML_OUT_DIR):
	mkdir -p $(HTML_OUT_DIR)

# Build Multi-page HTML version
html: $(SRC) | $(HTML_OUT_DIR)
	asciidoctor-multipage -D $(HTML_OUT_DIR) $(SRC)
	@echo "Multi-page HTML book generated in $(HTML_OUT_DIR)/"

# Build PDF version
pdf: $(SRC) | $(BUILD_DIR)
	@command -v asciidoctor-pdf >/dev/null 2>&1 || { echo >&2 "asciidoctor-pdf is required but not installed. Skipping PDF."; exit 0; }
	asciidoctor-pdf $(SRC) -o $(PDF_OUT)
	@echo "PDF book generated at $(PDF_OUT)"

# Clean up build artifacts
clean:
	rm -rf $(BUILD_DIR)
	@echo "Cleaned up build directory."

.PHONY: all html pdf clean

#ADOC_FLAGS = --require asciidoctor-diagram \
#             -a data-uri \
#             -a toc=left \
#             -a toclevels=2 \
#             -a source-highlighter=rouge \
#			 -a rouge-style=thankful_eyes \
#             -a plantuml-jar-path=$(HOME)/plantuml/plantuml.jar
#
#%.html: %.adoc Makefile
#	asciidoctor $(ADOC_FLAGS) $<
#
#book: sparta_book.adoc
#	asciidoctor $(ADOC_FLAGS) sparta_book.adoc
#
