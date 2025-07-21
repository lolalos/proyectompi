#include <mpi.h>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <cstdlib>
#include <algorithm>
#include <chrono>
#include <vector>
#include <numeric> // <-- Agregado para std::accumulate

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

// Función de segmentación (puedes cambiarla por otra si lo deseas)
void segment_image(const cv::Mat& input, cv::Mat& output, float sigma, float k, int min_size) {
  cv::pyrMeanShiftFiltering(input, output, sigma, k, 1);
}

// Obtener uso de memoria RAM en bytes
size_t get_memory_usage() {
#ifdef _WIN32
  PROCESS_MEMORY_COUNTERS info;
  GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info));
  return (size_t)info.WorkingSetSize;
#else
  struct rusage usage;
  getrusage(RUSAGE_SELF, &usage);
  return (size_t)usage.ru_maxrss * 1024L;
#endif
}

// Obtener número de núcleos de CPU
int get_cpu_cores() {
#ifdef _WIN32
  SYSTEM_INFO sysinfo;
  GetSystemInfo(&sysinfo);
  return sysinfo.dwNumberOfProcessors;
#else
  return sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

// Verificar si OpenCV tiene soporte CUDA
bool opencv_has_cuda() {
#ifdef HAVE_OPENCV_CUDAIMGPROC
  return true;
#else
  return false;
#endif
}

int main(int argc, char** argv) {
  MPI_Init(&argc, &argv);

  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  if (argc != 6) {
    if (rank == 0)
      std::cerr << "uso: " << argv[0] << " sigma k min input_image output_image\n";
    MPI_Finalize();
    return 1;
  }

  float sigma = std::atof(argv[1]);
  float k = std::atof(argv[2]);
  int min_size = std::atoi(argv[3]);
  std::string input_file = argv[4];
  std::string output_file = argv[5];

  cv::Mat input_img, output_img;
  int rows = 0, cols = 0, type = 0, elemSize = 0;

  if (rank == 0) {
    input_img = cv::imread(input_file, cv::IMREAD_COLOR);
    if (input_img.empty()) {
      std::cerr << "Error al cargar la imagen\n";
      MPI_Abort(MPI_COMM_WORLD, 1);
    }
    rows = input_img.rows;
    cols = input_img.cols;
    type = input_img.type();
    elemSize = input_img.elemSize();
  }

  MPI_Bcast(&rows, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&cols, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&type, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&elemSize, 1, MPI_INT, 0, MPI_COMM_WORLD);

  int local_rows = rows / size + (rank < rows % size ? 1 : 0);
  int start_row = (rows / size) * rank + std::min(rank, rows % size);

  cv::Mat local_input(local_rows, cols, type);
  cv::Mat local_output(local_rows, cols, type);

  if (rank == 0) {
    for (int i = 0; i < size; ++i) {
      int r = rows / size + (i < rows % size ? 1 : 0);
      int s = (rows / size) * i + std::min(i, rows % size);
      if (i == 0) {
        input_img.rowRange(s, s + r).copyTo(local_input);
      } else {
        MPI_Send(input_img.ptr(s), r * cols * elemSize, MPI_BYTE, i, 0, MPI_COMM_WORLD);
      }
    }
  } else {
    MPI_Recv(local_input.ptr(0), local_rows * cols * elemSize, MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  }

  // Medición de tiempo y memoria
  MPI_Barrier(MPI_COMM_WORLD);
  auto t0 = std::chrono::high_resolution_clock::now();
  size_t mem_before = get_memory_usage();

  segment_image(local_input, local_output, sigma, k, min_size);

  size_t mem_after = get_memory_usage();
  auto t1 = std::chrono::high_resolution_clock::now();
  double local_time = std::chrono::duration<double>(t1 - t0).count();
  size_t local_mem = mem_after > mem_before ? mem_after - mem_before : 0;
  int local_cells = local_rows * cols;

  // Recolecta la imagen segmentada
  if (rank == 0) {
    output_img = cv::Mat(rows, cols, type);
    local_output.copyTo(output_img.rowRange(start_row, start_row + local_rows));
    for (int i = 1; i < size; ++i) {
      int r = rows / size + (i < rows % size ? 1 : 0);
      int s = (rows / size) * i + std::min(i, rows % size);
      MPI_Recv(output_img.ptr(s), r * cols * elemSize, MPI_BYTE, i, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
  } else {
    MPI_Send(local_output.ptr(0), local_rows * cols * elemSize, MPI_BYTE, 0, 1, MPI_COMM_WORLD);
  }

  // Recolectar tiempos, memoria y celdas de todos los procesos
  std::vector<double> all_times(size);
  std::vector<size_t> all_mems(size);
  std::vector<int> all_cells(size);

  MPI_Gather(&local_time, 1, MPI_DOUBLE, all_times.data(), 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
  MPI_Gather(&local_mem, 1, MPI_UNSIGNED_LONG_LONG, all_mems.data(), 1, MPI_UNSIGNED_LONG_LONG, 0, MPI_COMM_WORLD);
  MPI_Gather(&local_cells, 1, MPI_INT, all_cells.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

  if (rank == 0) {
    cv::imwrite(output_file, output_img);

    double max_time = *std::max_element(all_times.begin(), all_times.end());
    double min_time = *std::min_element(all_times.begin(), all_times.end());
    double avg_time = std::accumulate(all_times.begin(), all_times.end(), 0.0) / size;

    size_t max_mem = *std::max_element(all_mems.begin(), all_mems.end());
    size_t min_mem = *std::min_element(all_mems.begin(), all_mems.end());
    double avg_mem = std::accumulate(all_mems.begin(), all_mems.end(), 0.0) / size;

    int total_cells = std::accumulate(all_cells.begin(), all_cells.end(), 0);

    std::cout << "=== Resultados de segmentación paralela ===\n";
    std::cout << "Procesos usados: " << size << "\n";
    std::cout << "Núcleos de CPU disponibles: " << get_cpu_cores() << "\n";
    std::cout << "Soporte de GPU (CUDA) en OpenCV: " << (opencv_has_cuda() ? "Sí" : "No") << "\n";
    std::cout << "Tamaño de la imagen: " << rows << " x " << cols << "\n";
    std::cout << "Celdas procesadas en total: " << total_cells << "\n";
    std::cout << "Celdas procesadas por proceso:\n";
    for (int i = 0; i < size; ++i)
      std::cout << "  Proceso " << i << ": " << all_cells[i] << "\n";
    std::cout << "Tiempo máximo de proceso: " << max_time << " s\n";
    std::cout << "Tiempo mínimo de proceso: " << min_time << " s\n";
    std::cout << "Tiempo promedio de proceso: " << avg_time << " s\n";
    std::cout << "Memoria máxima usada: " << (max_mem / (1024.0 * 1024.0)) << " MB\n";
    std::cout << "Memoria mínima usada: " << (min_mem / (1024.0 * 1024.0)) << " MB\n";
    std::cout << "Memoria promedio usada: " << (avg_mem / (1024.0 * 1024.0)) << " MB\n";
    std::cout << "Archivo de salida: " << output_file << "\n";
    std::cout << "==========================================\n";

    // Mostrar imágenes de entrada y salida
    cv::namedWindow("Imagen de entrada", cv::WINDOW_NORMAL);
    cv::imshow("Imagen de entrada", input_img);
    cv::namedWindow("Imagen segmentada", cv::WINDOW_NORMAL);
    cv::imshow("Imagen segmentada", output_img);
    std::cout << "Presiona cualquier tecla en las ventanas de imagen para continuar...\n";
    cv::waitKey(0);
    cv::destroyAllWindows();
  }

  MPI_Finalize();
  return 0;
}
