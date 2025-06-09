# OutlineTool - Architecture Documentation

This directory contains comprehensive PlantUML documentation explaining the design and architecture of the OutlineTool application.

## Documentation Overview

### 📋 Available Diagrams

| Diagram | File | Description |
|---------|------|-------------|
| **Class Architecture** | [`class-diagram.puml`](class-diagram.puml) | Shows the main classes, their relationships, and responsibilities |
| **Processing Sequence** | [`sequence-diagram.puml`](sequence-diagram.puml) | Illustrates the complete image processing workflow step-by-step |
| **Component Architecture** | [`component-diagram.puml`](component-diagram.puml) | High-level system components and their interactions |
| **Data Flow** | [`data-flow-diagram.puml`](data-flow-diagram.puml) | How data moves through the processing pipeline |
| **Deployment** | [`deployment-diagram.puml`](deployment-diagram.puml) | Build and runtime environment architecture |
| **Design Patterns** | [`design-patterns.puml`](design-patterns.puml) | Software design patterns used in the codebase |

### 🔍 Key Architectural Concepts

#### 1. **Modular Design**
- **ImageProcessor**: Pure static utility functions for image processing
- **DXFWriter**: Adapter for DXF file generation
- **Main**: Command-line interface and orchestration

#### 2. **Design Patterns Used**
- **Static Utility Pattern**: ImageProcessor with stateless functions
- **Parameter Object Pattern**: ProcessingParams to encapsulate configuration
- **Adapter Pattern**: DXFWriter bridges OpenCV and libdxfrw
- **Bridge Pattern**: Separation between high-level DXF operations and low-level library
- **Template Method Pattern**: Standardized processing pipeline

#### 3. **Data Processing Pipeline**
```
Image Input → Preprocessing → Boundary Detection → 
Perspective Correction → Noise Reduction → 
Object Extraction → DXF Export
```

### 🛠️ Viewing the Diagrams

#### Option 1: PlantUML Online Server
Visit [PlantUML Server](http://www.plantuml.com/plantuml/uml/) and paste the content of any `.puml` file.

#### Option 2: VS Code Extension
1. Install the "PlantUML" extension in VS Code
2. Open any `.puml` file
3. Press `Alt+D` to preview the diagram

#### Option 3: Local PlantUML Installation
```bash
# Install PlantUML (requires Java)
brew install plantuml  # macOS
# or
sudo apt install plantuml  # Ubuntu

# Generate PNG images
plantuml docs/*.puml

# Generate SVG images
plantuml -tsvg docs/*.puml
```

#### Option 4: Online PlantUML Editor
Use [PlantText](https://www.planttext.com/) for interactive editing and viewing.

### 📖 Diagram Descriptions

#### Class Diagram
- Shows the three main classes: `ImageProcessor`, `DXFWriter`, and `ProcessingParams`
- Illustrates inheritance from `DRW_Interface`
- Highlights the static utility pattern used throughout

#### Sequence Diagram
- Complete workflow from command-line input to DXF output
- Shows interactions between main components
- Illustrates error handling paths

#### Component Diagram
- High-level system architecture
- External dependencies (OpenCV, libdxfrw)
- Component responsibilities and interfaces

#### Data Flow Diagram
- Six-stage processing pipeline
- Data transformations at each stage
- Input validation and output generation

#### Deployment Diagram
- Build environment and dependencies
- Runtime requirements
- Distribution and packaging options

#### Design Patterns
- Detailed explanation of patterns used
- Benefits and trade-offs of each pattern
- Future extensibility considerations

### 🏗️ Architecture Principles

1. **Separation of Concerns**: Each class has a single responsibility
2. **Modularity**: Components can be developed and tested independently
3. **Stateless Design**: No mutable state, pure functional approach
4. **Dependency Inversion**: High-level modules don't depend on low-level details
5. **Interface Segregation**: Clean, focused interfaces for each component

### 🔮 Future Architecture Considerations

The current design supports easy extension for:
- Multiple image processing algorithms (Strategy pattern)
- Different export formats (Factory pattern)
- Batch processing capabilities
- Plugin architecture for custom filters
- GUI wrapper around the core processing engine

### 📚 Related Documentation

- [`../README.md`](../README.md) - User documentation and build instructions
- [`../CLAUDE.md`](../CLAUDE.md) - Developer guidance for Claude Code
- Source code in [`../src/`](../src/) and [`../include/`](../include/)

---

*Generated for OutlineTool v1.0.0 - A clean, modular approach to image-to-vector conversion*