# Contributing to irc.Brika.net

First of all, thank you for your interest in contributing to **irc.Brika.net**! Every contribution, whether it's a bug fix, a new feature, documentation, or an improvement, is appreciated.

## Getting Started

1. Fork the repository.

2. Clone your fork:

   ```bash
   git clone https://github.com/YOUR_USERNAME/ft_irc.git
   cd ft_irc
   ```

3. Create a new branch for your changes:

   ```bash
   git checkout -b feature/your-feature
   ```

4. Build the project and make sure it works before making changes:

   ```bash
   make
   ```

## How to Contribute

### Reporting Bugs

- Check existing issues before opening a new one.
- Describe the problem clearly.
- Include steps to reproduce the issue.
- Provide relevant error messages or logs when possible.

### Suggesting Features

- Explain the proposed feature and its purpose.
- Describe how it would improve the server.
- Open an issue to discuss significant changes before implementing them.

### Submitting Changes

1. Keep your changes focused and easy to review.
2. Follow the existing code style and project conventions.
3. Use clear, descriptive commit messages.
4. Test your changes and ensure the project builds successfully.
5. Push your branch to your fork.
6. Open a Pull Request against the `develop` branch, unless the maintainers specify otherwise.

## Development Guidelines

- Use **C++98** and avoid features from newer C++ standards.
- Follow the existing architecture and naming conventions.
- Keep the code readable, modular, and maintainable.
- Handle errors and invalid client input appropriately.
- Avoid introducing unnecessary dependencies.
- Do not include unrelated changes in your Pull Request.
- Never commit passwords, private keys, or other sensitive information.

## Commit Messages

Use clear commit messages that describe the purpose of each change.

Examples:

```text
feat(server): improve client connection handling
fix(auth): reject invalid server passwords
fix(protocol): handle malformed IRC commands
docs: update connection instructions
refactor(client): simplify client management
```

## Pull Request Checklist

Before submitting your Pull Request, make sure that:

- [ ] The project compiles successfully.
- [ ] Your changes have been tested.
- [ ] Existing functionality has not been unintentionally broken.
- [ ] Your code follows the project's conventions.
- [ ] Your Pull Request explains what changed and why.
- [ ] You have updated the documentation if necessary.

## Questions and Discussions

If you have questions, ideas, or need clarification, feel free to open an issue in the repository.

Thank you for helping improve **irc.Brika.net**!

