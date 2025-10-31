# Contributing to Luma

Thank you for your interest in contributing to Luma, a simple yet powerful systems programming language!

This document outlines the guidelines and best practices to help you contribute effectively. Whether you're reporting bugs, suggesting features, or submitting code, we appreciate your help making Luma better for everyone.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [How to Contribute](#how-to-contribute)
  - [Reporting Issues](#reporting-issues)
  - [Suggesting Features](#suggesting-features)
  - [Submitting Code](#submitting-code)
- [Development Setup](#development-setup)
- [Style Guide](#style-guide)
- [Testing](#testing)
- [License](#license)

## Code of Conduct

By participating in this project, you agree to abide by the Contributor Covenant Code of Conduct. We strive to foster a welcoming and inclusive community.

## How to Contribute

### Reporting Issues

If you find a bug or unexpected behavior in Luma, please open an issue on the GitHub repository with:

- A clear and descriptive title
- Steps to reproduce the problem
- Expected and actual behavior
- Any relevant code snippets or error messages

### Suggesting Features

We welcome new ideas! To suggest a feature:

- Open a new issue with the `[Feature Request]` prefix in the title
- Describe the feature, its use cases, and why it would be valuable

### Submitting Code

We accept contributions in the form of pull requests. Here's how:

1. Fork the repository and clone your fork locally
2. Create a feature branch for your changes
3. Follow the [Style Guide](#style-guide) for coding standards
4. Add tests where applicable
5. Run all existing tests and ensure they pass
6. Commit your changes with clear messages
7. Push your branch and open a pull request targeting the main branch

We will review your pull request and provide feedback.

## Development Setup

To set up a local development environment:

1. Ensure you have the required dependencies installed (e.g., cmake, ninja, gcc or your preferred compiler).

2. Clone the Luma repository:

```bash
git clone https://github.com/your-username/luma.git
cd luma
```

3. Build the project following instructions in the README (or specific build scripts).

4. Run to ensure everything is working:

```bash
./luma
```

## Style Guide

- Follow consistent indentation (4 spaces recommended)
- Use descriptive variable and function names
- Write clear and concise comments where necessary
- Use the existing code style as a reference (e.g., brace placement, spacing)
- Prefer explicit error handling and avoid undefined behavior

## Testing

Tests are important to maintain code quality. Before submitting a pull request:

- Write new tests for your features or bug fixes
- Run the full test suite to make sure everything passes
- Avoid breaking existing tests

## License

By contributing, you agree that your contributions will be licensed under the MIT License.

---

**Thank you for helping make Luma awesome!**
