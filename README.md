# Congestion control API project

This repo contains several attempts to define programmable NIC congestion control API:
1. `v0-api` **old**: contains the first attempt to understand varius algorithms in Linux kernel-like API style
2. `v1-api` **old**: contains the second attept, that operates on signals/triggers/controls
3. `pcm` **current**: is the current version that build on top of `v1-api` ideas but implements Programmable Congestion Management (PCM) API proposal.