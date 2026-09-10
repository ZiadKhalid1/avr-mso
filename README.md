# avr-mso
```python
markdown_content = """# Git & GitHub Workflow Guide: Branch, Commit, Push, and Pull

A practical, step-by-step guide for standard Git branching workflows.

---

## 1. Create and Switch to a New Branch

Always make sure your local `main` branch is up to date before creating a new branch:

```bash
# Switch to main branch
git checkout main

# Fetch and merge latest remote changes
git pull origin main

```

Create and switch to your new branch:

```bash
# Using the classic checkout command
git checkout -b feature-name

# Or using the modern switch command (Git 2.23+)
git switch -c feature-name

```

Verify your active branch:

```bash
git branch

```

---

## 2. Make Changes & Stage Files

Inspect modified and untracked files:

```bash
git status

```

Stage specific files:

```bash
git add path/to/file.c

```

Or stage all changed files:

```bash
git add .

```

---

## 3. Commit Your Changes

Commit staged changes with a clear, descriptive message:

```bash
git commit -m "feat: implement initial logic"

```

To view your recent commit history:

```bash
git log --oneline -n 5

```

---

## 4. Push Branch to GitHub

When pushing a newly created local branch for the first time, link it to the remote tracking branch using `-u` (or `--set-upstream`):

```bash
git push -u origin feature-name

```

For subsequent pushes on this branch, run:

```bash
git push

```

---

## 5. Pull Remote Changes

Keep your current branch synchronized with updates pushed to GitHub:

```bash
# Pull changes for the current tracking branch
git pull

```

To incorporate the latest updates from `main` into your feature branch:

```bash
git pull origin main

```

---

## Quick Reference Sheet

| Action | Command |
| --- | --- |
| **Create & switch to new branch** | `git checkout -b <branch-name>` |
| **Switch existing branch** | `git checkout <branch-name>` or `git switch <branch-name>` |
| **Check repository status** | `git status` |
| **Stage changes** | `git add <file>` or `git add .` |
| **Commit staged changes** | `git commit -m "<message>"` |
| **Push new branch (first time)** | `git push -u origin <branch-name>` |
| **Push subsequent commits** | `git push` |
| **Pull changes from remote** | `git pull` |
| **Pull changes from main** | `git pull origin main` |
| **List local branches** | `git branch` |
| """ |  |

filename = "git-workflow-guide.md"
with open(filename, "w", encoding="utf-8") as f:
f.write(markdown_content)

print(f"Generated {filename}")

```

```text?code_stdout&code_event_index=1
Generated git-workflow-guide.md


```

Your Markdown (.md) file is ready:


Here is the formatted Markdown content ready to paste directly into your `README.md`:

```markdown
# Git & GitHub Workflow Guide: Branch, Commit, Push, and Pull

A practical, step-by-step guide for standard Git branching workflows.

---

## 1. Create and Switch to a New Branch

Always make sure your local `main` branch is up to date before creating a new branch:

```bash
# Switch to main branch
git checkout main

# Fetch and merge latest remote changes
git pull origin main

```

Create and switch to your new branch:

```bash
# Using the classic checkout command
git checkout -b feature-name

# Or using the modern switch command (Git 2.23+)
git switch -c feature-name

```

Verify your active branch:

```bash
git branch

```

---

## 2. Make Changes & Stage Files

Inspect modified and untracked files:

```bash
git status

```

Stage specific files:

```bash
git add path/to/file.c

```

Or stage all changed files:

```bash
git add .

```

---

## 3. Commit Your Changes

Commit staged changes with a clear, descriptive message:

```bash
git commit -m "feat: implement initial logic"

```

To view your recent commit history:

```bash
git log --oneline -n 5

```

---

## 4. Push Branch to GitHub

When pushing a newly created local branch for the first time, link it to the remote tracking branch using `-u` (or `--set-upstream`):

```bash
git push -u origin feature-name

```

For subsequent pushes on this branch, run:

```bash
git push

```

---

## 5. Pull Remote Changes

Keep your current branch synchronized with updates pushed to GitHub:

```bash
# Pull changes for the current tracking branch
git pull

```

To incorporate the latest updates from `main` into your feature branch:

```bash
git pull origin main

```

---

## Quick Reference Sheet

| Action | Command |
| --- | --- |
| **Create & switch to new branch** | `git checkout -b <branch-name>` |
| **Switch existing branch** | `git checkout <branch-name>` or `git switch <branch-name>` |
| **Check repository status** | `git status` |
| **Stage changes** | `git add <file>` or `git add .` |
| **Commit staged changes** | `git commit -m "<message>"` |
| **Push new branch (first time)** | `git push -u origin <branch-name>` |
| **Push subsequent commits** | `git push` |
| **Pull changes from remote** | `git pull` |
| **Pull changes from main** | `git pull origin main` |
| **List local branches** | `git branch` |

```

```
