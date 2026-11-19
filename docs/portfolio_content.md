# Portfolio content

Source material for `site/index.html` (Phase 6). Pulled from:

- **Resume:** [`james-loke-resume-2-11-26.pdf`](../james-loke-resume-2-11-26.pdf) (dated 2 Nov 2026)
- **GitHub:** https://github.com/BigBouncyBoii (profile has no bio; 18 public repos)
- **LinkedIn:** https://www.linkedin.com/in/james-loke-34a0612bb/ — **not extracted** (LinkedIn blocks automated fetches). To fill the gaps, use LinkedIn → More → Save to PDF, drop the file in the repo, and merge anything new (headline, About, volunteering, extra roles) into this file.

When the resume, GitHub or LinkedIn changes, update this file first, then the site.

---

## Identity & links

| | |
|---|---|
| Name | James Loke |
| Tagline (draft) | Computer Science @ UCL. Systems, performance and ML inference. |
| Location | London, UK (from Petaling Jaya, Malaysia) |
| Email | jameslokejw@gmail.com (your choice whether to show it on the public page) |
| Phone | On the resume. **Leave it off the public site.** |
| GitHub | https://github.com/BigBouncyBoii |
| LinkedIn | https://www.linkedin.com/in/james-loke-34a0612bb/ |
| Resume PDF | `james-loke-resume-2-11-26.pdf` (copy into `site/` so Pages can serve it) |
| This repo | https://github.com/BigBouncyBoii/portfolio_page |

**Short bio (draft, based on the resume):**
Third-year Computer Science student at UCL (predicted First; 85% in Year 1, 80% in Year 2). I like systems and performance work: async runtimes, data pipelines, HPC benchmarking and real-time ML inference on edge hardware. Previously a Quantitative Technology Intern at Qube Research & Technologies and a Software Engineering Intern at Grab.

---

## Education

**BSc Computer Science, University College London** (London, UK, 2024 – 2027)
- Predicted First Class Honours; Year 1: 85%, Year 2: 80%
- Relevant modules: Computer Architecture & Concurrency, Systems Engineering, Machine Learning for Visual Computing, Machine Learning for Robotics, Algorithms, OOP

**A-Level & IGCSE: Sunway College, SriKDU International School** (Petaling Jaya, Malaysia, 2017 – 2024)
- A-Level: 4A* (average 96%)
- IGCSE: 11A* (average 96%)

---

## Experience

### UCL High Performance Computing team member, CIUK Cluster Challenge
University College London, London. 10/2026 – Present
- Ran and scaled the HPL and HPCG benchmarks on an HPC cluster with Linux and Slurm batch scripts, setting up job scheduling, MPI parallelism and resource allocation across a supercomputer.
- Benchmarked HPL dense matrix operations and HPCG sparse conjugate-gradient workloads, measuring performance, scalability and energy efficiency on 1–4 nodes.
- Related repo (private): `CIUK-UCLusters`, "Repository for hpc comp"

### Quantitative Technology Intern (SWE)
Qube Research & Technologies (QRT), London. 06/2026 – 09/2026
- Re-architected an AI research pipeline into an asynchronous multi-agent runtime using **Pydantic AI**, with durable execution, deterministic replay, fault recovery, parallel agent execution, planning, and browser/internal tool integration.
- Made a 100M-record data pipeline **12× faster** by pushing Pandas transformations down into SQL, streaming psycopg2 queries to disk, compressing with gzip across multiple processes, and using finer-grained threading.
- Built a scalable reconciliation pipeline for **1B+ records/day** across multiple regions, using multiprocessing and **DuckDB** to detect row-level data discrepancies.

### Software Engineering Contractor, UCL IXN
IBM x Tech To The Rescue x Dignitas, London. 10/2025 – 03/2026
- Built a RAG-powered AI agent with **LangGraph** and **LangChain**, backed by Qdrant and Redis, plus an **MCP** server hosting WhatsApp, translation and SQL tools.
- Fine-tuned Meta **NLLB** with **LoRA** on a 200K-sample English–Swahili dataset, converted it to **ONNX** and applied INT8 quantisation with ONNX Runtime. This cut model-weight memory by about **65%** and made CPU inference possible.
- Designed and deployed a multi-service architecture with **Docker** on **Azure Container Apps**: 4 containers with CI/CD pipelines.

### Software Engineering Intern
Grab (Southeast Asia's largest tech company), Petaling Jaya, Malaysia. 07/2025 – 09/2025
- Extended **Go** backend services for large-scale configuration changes, using **gRPC** to communicate across 10+ microservices. Cut one API's latency by **30%** with lazy loading, caching and concurrent processing.
- Built UI with **React** and **Tailwind CSS** for large-scale analytics tools and an interactive map that replays taxi journeys. Wrote unit and integration tests with Jest and Enzyme.
- Instrumented 3 core web services with **Datadog RUM** to monitor performance and crashes.
- Related public repo: [`grab_terraform`](https://github.com/BigBouncyBoii/grab_terraform), a Terraform exercise provisioning EC2 instances in eu-west-2 and ap-southeast-1

### Programmes
Palantir Spring Week, Barclays Spring Week, Amazon Web Services Insight Day, YC Startup School (London, 04/2025 – 08/2026)

---

## Projects

### Featured (resume)

**Deterministic Real-Time Scheduling of Vision-Language-Action Models for Edge Robotic Manipulation** (09/2026 – Present; dissertation, repo `dissertation` is private)
- Designing an event-driven, asynchronous dual-stream VLA architecture for resource-constrained edge hardware, separating the vision-language perception backbone from the time-critical action head.
- Optimising GPU execution with CUDA Streams and TensorRT, using attention maps and pixel-level changes to decide when to re-run perception and avoid redundant inference.
- Building a C++ benchmarking framework that compares synchronous and asynchronous inference on end-to-end latency, control-frequency stability, GPU utilisation, energy use and task success.

**Portfolio activity tracker (this repo)**: [portfolio_page](https://github.com/BigBouncyBoii/portfolio_page)
- C++17 Windows tracker using low-level keyboard/mouse hooks (counts only), 1 Hz foreground-app sampling with idle/lock detection, SQLite per-minute buckets and a tray app. It publishes sanitised JSON that powers this site's live stats. See `README.md`.

### From GitHub (public repos, newest first)

| Project | Lang | What it is |
|---|---|---|
| [Machine-learning-for-robotics-cw1](https://github.com/BigBouncyBoii/Machine-learning-for-robotics-cw1) | Python | UCL ELEC0144 group coursework: fruit localisation with from-scratch image processing, a NumPy 1-3-1 tanh network (SGD vs ADAM), and an open exploration on whether a camera can tell a light grip from a firm one |
| [async_python_scratch](https://github.com/BigBouncyBoii/async_python_scratch) | Python | "bsync": an asyncio-style event loop written from scratch (tasks, futures, step scheduling) to learn how async works under the hood |
| [tetris-AI](https://github.com/BigBouncyBoii/tetris-AI) | – | AI that plays Tetris: genetic algorithm, lookahead and hill-climbing variants |
| [SampleServiceHackEurope](https://github.com/BigBouncyBoii/SampleServiceHackEurope) | Python/Docker | HackEurope: a deliberately insecure sample service used as a target for the team's security project |
| [Rust_P2P_job_scheduler](https://github.com/BigBouncyBoii/Rust_P2P_job_scheduler) | Rust | Simulated decentralised peer-to-peer job scheduler for AI workloads on GPU/CPU clusters; avoids a single-point-of-failure central scheduler |
| [cf_ai_james_loke](https://github.com/BigBouncyBoii/cf_ai_james_loke) | TypeScript | Event Planner Assistant: a conversational agent on Cloudflare Workers AI with web search and Slack integration. Live demo: https://event-planner.jameslokejw.workers.dev |
| [Jetbrains-code-DNA](https://github.com/BigBouncyBoii/Jetbrains-code-DNA) | Java | CLI that analyses JetBrains plugin artifacts (ZIP/JAR) into JSON reports, with optional HMAC-SHA256 per entry and similarity comparison |
| [Jetbrains-post-tagger](https://github.com/BigBouncyBoii/Jetbrains-post-tagger) | Python | Async task processing with Celery + Redis behind a Flask API (pi to n digits), Docker Compose |
| [Jetbrains-monitoring-as-code](https://github.com/BigBouncyBoii/Jetbrains-monitoring-as-code) | Shell | Prometheus vs Grafana alerting demo (Alertmanager, Loki log alerts, notification templates), Docker Compose |
| [redis_go](https://github.com/BigBouncyBoii/redis_go) | Go | Redis-style server in Go |
| [goapi](https://github.com/BigBouncyBoii/goapi) | Go | REST API in Go |
| [grab_terraform](https://github.com/BigBouncyBoii/grab_terraform) | HCL | Terraform IaC: multi-region AWS EC2 provisioning |
| [SUSTECH](https://github.com/BigBouncyBoii/SUSTECH) | – | Fintech hackathon entry (zip archive) |
| [RoadSigns](https://github.com/BigBouncyBoii/RoadSigns) | Python | CS50AI: traffic-sign classifier (neural network) |
| [Chatter](https://github.com/BigBouncyBoii/Chatter) | Python/JS | CS50W final project: a chat web app |
| [Shopping](https://github.com/BigBouncyBoii/Shopping) | Python | CS50AI: predicts online-shopper purchases (k-NN) |
| [learn-rust](https://github.com/BigBouncyBoii/learn-rust) | – | Rust learning notes (empty so far) |

Private repos (don't link): `dissertation`, `CIUK-UCLusters`, `edu_nexus_project`, `algorithmsCoursework`, `hacklondon2025`, `C_Coursework`.

Descriptions for `redis_go`, `goapi`, `RoadSigns`, `Shopping` and `SUSTECH` come from the repo names and file layout only, because their READMEs are thin or missing. Check them before publishing.

**Suggested featured set for the page:** VLA dissertation, this tracker, async_python_scratch, Rust_P2P_job_scheduler, cf_ai_james_loke (live demo), tetris-AI, redis_go.

---

## Skills

- **Languages:** C++, Python, Go, Java, SQL, JavaScript/TypeScript (GitHub also shows Rust, C, HCL)
- **Systems & performance:** concurrency, distributed systems, networking, Linux, performance optimisation, benchmarking, Slurm, HPC
- **ML & inference:** PyTorch, ONNX Runtime, model quantisation, CUDA, TensorRT, Pydantic AI, LangGraph, LangChain
- **Infrastructure & backend:** AWS, Azure, Docker, FastAPI, gRPC, CI/CD, Git (GitHub also shows Terraform, Prometheus/Grafana, Celery/Redis, Cloudflare Workers)

---

## Awards

- **Finalist**, Palantir Winter Tech Fellowship
- **Top 10**, Imperial x UCL Entrepreneurs First Hackathon
- Jeffrey Cheah Entrance Scholarship (**100% tuition waiver**)
- Distinction, University of Waterloo Euclid (CEMC) Contest
- Distinction, Xiamen University Chen Jingrun Cup (**75% scholarship award**)
- Bronze, Senior Kangaroo Math

## Certificates

- FreeCodeCamp: Learn CUDA Programming
- Coursera: Unix System Basics
- NVIDIA: Introduction to Networking
- IELTS: 8.5
- Harvard CS50: Introduction to Computer Science
- Harvard CS50W: Web Programming
- Harvard CS50AI: Introduction to Artificial Intelligence
- learncpp.com
- Palantir Foundry Foundations
