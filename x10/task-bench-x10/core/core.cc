/* Copyright 2018 Stanford University
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <map>
#include <string>

#include "core.h"
#include "core_kernel.h"

void Kernel::execute() const
{
  switch(type) {
  case KernelType::EMPTY:
    execute_kernel_empty(*this);
    break;
  case KernelType::BUSY_WAIT:
    execute_kernel_busy_wait(*this);
    break;
  default:
    assert(false && "unimplemented kernel type");
  };
}

static const std::map<std::string, KernelType> &ktype_by_name()
{
  static std::map<std::string, KernelType> types;

  if (types.empty()) {
    types["empty"] = KernelType::EMPTY;
    types["busy_wait"] = KernelType::BUSY_WAIT;
    types["memory_bound"] = KernelType::MEMORY_BOUND;
    types["compute_bound"] = KernelType::COMPUTE_BOUND;
  }

  return types;
}

static const std::map<KernelType, std::string> &name_by_ktype()
{
  static std::map<KernelType, std::string> names;

  if (names.empty()) {
    auto types = ktype_by_name();
    for (auto pair : types) {
      names[pair.second] = pair.first;
    }
  }

  return names;
}

static const std::map<std::string, DependenceType> &dtype_by_name()
{
  static std::map<std::string, DependenceType> types;

  if (types.empty()) {
    types["trivial"] = DependenceType::TRIVIAL;
    types["no_comm"] = DependenceType::NO_COMM;
    types["stencil_1d"] = DependenceType::STENCIL_1D;
    types["stencil_1d_periodic"] = DependenceType::STENCIL_1D_PERIODIC;
    types["dom"] = DependenceType::DOM;
    types["tree"] = DependenceType::TREE;
    types["fft"] = DependenceType::FFT;
    types["all_to_all"] = DependenceType::ALL_TO_ALL;
  }

  return types;
}

static const std::map<DependenceType, std::string> &name_by_dtype()
{
  static std::map<DependenceType, std::string> names;

  if (names.empty()) {
    auto types = dtype_by_name();
    for (auto pair : types) {
      names[pair.second] = pair.first;
    }
  }

  return names;
}

long TaskGraph::offset_at_timestep(long timestep) const
{
  switch (dependence) {
  case DependenceType::TRIVIAL:
  case DependenceType::NO_COMM:
  case DependenceType::STENCIL_1D:
  case DependenceType::STENCIL_1D_PERIODIC:
    return 0;
  case DependenceType::DOM:
    return std::max(0L, timestep + max_width - timesteps);
  case DependenceType::TREE:
  case DependenceType::FFT:
  case DependenceType::ALL_TO_ALL:
    return 0;
  default:
    assert(false && "unexpected dependence type");
  };
}

long TaskGraph::width_at_timestep(long timestep) const
{
  switch (dependence) {
  case DependenceType::TRIVIAL:
  case DependenceType::NO_COMM:
  case DependenceType::STENCIL_1D:
  case DependenceType::STENCIL_1D_PERIODIC:
    return max_width;
  case DependenceType::DOM:
    return std::min(max_width,
                    std::min(timestep + 1, timesteps - timestep));
  case DependenceType::TREE:
    return std::min(max_width, 1L << std::min(timestep, 62L));
  case DependenceType::FFT:
  case DependenceType::ALL_TO_ALL:
    return max_width;
  default:
    assert(false && "unexpected dependence type");
  };
}

long TaskGraph::max_dependence_sets() const
{
  switch (dependence) {
  case DependenceType::TRIVIAL:
  case DependenceType::NO_COMM:
  case DependenceType::STENCIL_1D:
  case DependenceType::STENCIL_1D_PERIODIC:
  case DependenceType::DOM:
  case DependenceType::TREE:
    return 1;
  case DependenceType::FFT:
    return (long)ceil(log2(max_width));
  case DependenceType::ALL_TO_ALL:
    return 1;
  default:
    assert(false && "unexpected dependence type");
  };
}

long TaskGraph::timestep_period() const
{
  // For all the dependence types currently defined, the pattern
  // repeats with a period equal to the number of timesteps.
  return max_dependence_sets();
}

long TaskGraph::dependence_set_at_timestep(long timestep) const
{
  switch (dependence) {
  case DependenceType::TRIVIAL:
  case DependenceType::NO_COMM:
  case DependenceType::STENCIL_1D:
  case DependenceType::STENCIL_1D_PERIODIC:
  case DependenceType::DOM:
  case DependenceType::TREE:
    return 0;
  case DependenceType::FFT:
    return (timestep + max_dependence_sets() - 1) % max_dependence_sets();
  case DependenceType::ALL_TO_ALL:
    return 0;
  default:
    assert(false && "unexpected dependence type");
  };
}

std::vector<std::pair<long, long> > TaskGraph::reverse_dependencies(long dset, long point) const
{
  std::vector<std::pair<long, long> > deps;

  switch (dependence) {
  case DependenceType::TRIVIAL:                                                                                                                                           
    break;
  case DependenceType::NO_COMM:                                                                                                                                           
    deps.push_back(std::pair<long, long>(point, point));
    break;
  case DependenceType::STENCIL_1D:                                                                                                                                        
    deps.push_back(std::pair<long, long>(std::max(0L, point-1), std::min(point+1, max_width-1)));
    break;
  case DependenceType::STENCIL_1D_PERIODIC:                                                                                                                              
    deps.push_back(std::pair<long, long>(std::max(0L, point-1), std::min(point+1, max_width-1)));
    if (point-1 < 0) { // Wrap around negative case                                                                                                                              
      deps.push_back(std::pair<long, long>(max_width-1, max_width-1));
    }
    if (point+1 >= max_width) { // Wrap around positive case                                                                                                                     
      deps.push_back(std::pair<long, long>(0, 0));
    }
    break;
  case DependenceType::DOM:
    deps.push_back(std::pair<long, long>(point, std::min(max_width-1, point+1)));
    break;
  case DependenceType::TREE:
    {
      long child1 = point*2;
      long child2 = point*2 + 1;
      if (child1 < max_width && child2 < max_width)
        deps.push_back(std::pair<long, long>(child1, child2));
      else if (child1 < max_width)
        deps.push_back(std::pair<long, long>(child1, child1));

    }
    break;
    case DependenceType::FFT:                                                                                                                                               
    {
      long d1 = point - (1 << dset);
      long d2 = point + (1 << dset);
      if (d1 >= 0) {
        deps.push_back(std::pair<long, long>(d1, d1));
      }
      if (d2 < max_width) {
        deps.push_back(std::pair<long, long>(d2, d2));
      }
    }
    break;
  case DependenceType::ALL_TO_ALL:                                                                                                                                        
    deps.push_back(std::pair<long, long>(0, max_width-1));
    break;
  default:
    assert(false && "unexpected dependence type");
  };

  return deps;
}

std::vector<std::pair<long, long> > TaskGraph::dependencies(long dset, long point) const
{
  std::vector<std::pair<long, long> > deps;

  switch (dependence) {
  case DependenceType::TRIVIAL:
    break;
  case DependenceType::NO_COMM:
    deps.push_back(std::pair<long, long>(point, point));
    break;
  case DependenceType::STENCIL_1D:
    deps.push_back(std::pair<long, long>(std::max(0L, point-1), std::min(point+1, max_width-1)));
    break;
  case DependenceType::STENCIL_1D_PERIODIC:
    deps.push_back(std::pair<long, long>(std::max(0L, point-1), std::min(point+1, max_width-1)));
    if (point-1 < 0) { // Wrap around negative case
      deps.push_back(std::pair<long, long>(max_width-1, max_width-1));
    }
    if (point+1 >= max_width) { // Wrap around positive case
      deps.push_back(std::pair<long, long>(0, 0));
    }
    break;
  case DependenceType::DOM:
    deps.push_back(std::pair<long, long>(std::max(0L, point-1), point));
    break;
  case DependenceType::TREE:
    {
      long parent = point/2;
      deps.push_back(std::pair<long, long>(parent, parent));
    }
    break;
  case DependenceType::FFT:
    {
      long d1 = point - (1 << dset);
      long d2 = point + (1 << dset);
      if (d1 >= 0) {
        deps.push_back(std::pair<long, long>(d1, d1));
      }
      if (d2 < max_width) {
        deps.push_back(std::pair<long, long>(d2, d2));
      }
    }
    break;
  case DependenceType::ALL_TO_ALL:
    deps.push_back(std::pair<long, long>(0, max_width-1));
    break;
  default:
    assert(false && "unexpected dependence type");
  };

  return deps;
}

void TaskGraph::execute_point(long timestep, long point,
                              char *output_ptr, size_t output_bytes,
                              const char **input_ptr, const size_t *input_bytes,
                              size_t n_inputs) const
{
  // Validate timestep and point
  assert(0 <= timestep && timestep < timesteps);

  long offset = offset_at_timestep(timestep);
  long width = width_at_timestep(timestep);
  assert(offset <= point && point < offset+width);

  long last_offset = timestep > 0 ? offset_at_timestep(timestep-1) : 0;
  long last_width = timestep > 0 ? width_at_timestep(timestep-1) : 0;

  // Validate input
  {
    size_t idx = 0;
    long dset = dependence_set_at_timestep(timestep);
    std::vector<std::pair<long, long> > deps = dependencies(dset, point);
    for (auto span : deps) {
      for (long dep = span.first; dep <= span.second; dep++) {
        if (last_offset <= dep && dep < last_offset + last_width) {
          assert(idx < n_inputs);

          assert(input_bytes[idx] == output_bytes_per_task);
          assert(input_bytes[idx] >= sizeof(std::pair<long, long>));

          const std::pair<long, long> input = *reinterpret_cast<const std::pair<long, long> *>(input_ptr[idx]);
          assert(input.first == timestep - 1);
          assert(input.second == dep);
          idx++;
        }
      }
    }
    // FIXME (Elliott): Legion is currently passing in uninitialized
    // memory for dependencies outside of the last offset/width.
    // assert(idx == n_inputs);
  }

  // Validate output
  assert(output_bytes == output_bytes_per_task);
  assert(output_bytes >= sizeof(std::pair<long, long>));

  // Generate output
  std::pair<long, long> *output = reinterpret_cast<std::pair<long, long> *>(output_ptr);
  output->first = timestep;
  output->second = point;

  // Execute kernel
  Kernel k(kernel);
  k.execute();
}

static TaskGraph default_graph()
{
  TaskGraph graph;

  graph.timesteps = 4;
  graph.max_width = 4;
  graph.dependence = DependenceType::TRIVIAL;
  graph.kernel = {KernelType::EMPTY, 0};
  graph.output_bytes_per_task = sizeof(std::pair<long, long>);

  return graph;
}

void needs_argument(int i, int argc, const char *flag) {
  if (i+1 >= argc) {
    fprintf(stderr, "error: Flag \"%s\" requires an argument\n", flag);
    abort();
  }
}

App::App(int argc, char **argv)
  : verbose(false)
{
  TaskGraph graph = default_graph();

  // Parse command line
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-v")) {
      verbose = true;
    }

    if (!strcmp(argv[i], "-steps")) {
      needs_argument(i, argc, "-steps");
      long value = atol(argv[++i]);
      if (value <= 0) {
        fprintf(stderr, "error: Invalid flag \"-steps %ld\" must be > 0\n", value);
        abort();
      }
      graph.timesteps = value;
    }

    if (!strcmp(argv[i], "-width")) {
      needs_argument(i, argc, "-width");
      long value = atol(argv[++i]);
      if (value <= 0) {
        fprintf(stderr, "error: Invalid flag \"-width %ld\" must be > 0\n", value);
        abort();
      }
      graph.max_width = value;
    }

    if (!strcmp(argv[i], "-type")) {
      needs_argument(i, argc, "-type");
      auto types = dtype_by_name();
      auto name = argv[++i];
      auto type = types.find(name);
      if (type == types.end()) {
        fprintf(stderr, "error: Invalid flag \"-type %s\"\n", name);
        abort();
      }
      graph.dependence = type->second;
    }

    if (!strcmp(argv[i], "-kernel")) {
      needs_argument(i, argc, "-kernel");
      auto types = ktype_by_name();
      auto name = argv[++i];
      auto type = types.find(name);
      if (type == types.end()) {
        fprintf(stderr, "error: Invalid flag \"-kernel %s\"\n", name);
        abort();
      }
      graph.kernel.type = type->second;
    }

    if (!strcmp(argv[i], "-iter")) {
      needs_argument(i, argc, "-iter");
      long value = atol(argv[++i]);
      if (value < 0) {
        fprintf(stderr, "error: Invalid flag \"-iter %ld\" must be >= 0\n", value);
        abort();
      }
      graph.kernel.iterations = value;
    }

    if (!strcmp(argv[i], "-and")) {
      graphs.push_back(graph);
      graph = default_graph();
    }
  }

  graphs.push_back(graph);

  check();
}

void App::check() const
{
  // Validate task graph is well-formed
  for (auto g : graphs) {
    for (long t = 0; t < g.timesteps; ++t) {
      long offset = g.offset_at_timestep(t);
      long width = g.width_at_timestep(t);
      assert(offset >= 0);
      assert(width >= 0);
      assert(offset + width <= g.max_width);

      long dset = g.dependence_set_at_timestep(t);
      assert(dset >= 0 && dset <= g.max_dependence_sets());
    }
  }
}

void App::display() const
{
  printf("Running Task Benchmark\n");
  printf("  Configuration:\n");
  int i = 0;
  for (auto g : graphs) {
    ++i;

    auto knames = name_by_ktype();
    auto dnames = name_by_dtype();

    printf("    Task Graph %d:\n", i);
    printf("      Time Steps: %ld\n", g.timesteps);
    printf("      Max Width: %ld\n", g.max_width);
    printf("      Dependence Type: %s\n", dnames.at(g.dependence).c_str());
    printf("      Kernel:\n");
    printf("        Type: %s\n", knames.at(g.kernel.type).c_str());
    printf("        Iterations: %ld\n", g.kernel.iterations);

    if (verbose) {
      for (long t = 0; t < g.timesteps; ++t) {
        long offset = g.offset_at_timestep(t);
        long width = g.width_at_timestep(t);
        long dset = g.dependence_set_at_timestep(t);
        printf("      Dependencies (offset %ld, width %ld):\n",
               g.offset_at_timestep(t), g.width_at_timestep(t));
        for (long p = offset; p < offset + width; ++p) {
          printf("        Point %ld:", p);
          auto deps = g.dependencies(dset, p);
          for (auto dep : deps) {
            for (long dp = dep.first; dp <= dep.second; ++dp) {
              printf(" %ld", dp);
            }
          }
          printf("\n");
        }
      }
    }
  }
}

void App::report_timing(double elapsed_seconds) const
{
  long long num_tasks = 0;
  long long num_deps = 0;
  for (auto g : graphs) {
    for (long t = 0; t < g.timesteps; ++t) {
      long offset = g.offset_at_timestep(t);
      long width = g.width_at_timestep(t);
      long dset = g.dependence_set_at_timestep(t);

      num_tasks += width;

      for (long p = offset; p < offset + width; ++p) {
        auto deps = g.dependencies(dset, p);
        for (auto dep : deps) {
          num_deps += dep.second - dep.first + 1;
        }
      }
    }
  }

  printf("Total Tasks %lld\n", num_tasks);
  printf("Total Dependencies %lld\n", num_deps);
  printf("Elapsed Time %e seconds\n", elapsed_seconds);
  printf("Time per Task %e seconds\n", elapsed_seconds/num_tasks);
  printf("Time per Dependency %e seconds\n", elapsed_seconds/num_deps);
}
