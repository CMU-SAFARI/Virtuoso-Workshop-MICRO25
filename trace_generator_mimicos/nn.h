#include <iostream>
#include <vector>
#include <cmath>
#include <numeric>
#include <algorithm>

using namespace std;

using Vector = std::vector<double>;
using Matrix = std::vector<std::vector<double>>;

// @hsongaraVTW25: Activation Functions

// ReLU(x) = max(0, x) applied element-wise to the matrix
Matrix relu(const Matrix& Z) {
    Matrix A = Z;
    for (auto& row : A) {
        for (double& val : row) {
            val = std::max(0.0, val);
        }
    }
    return A;
}

// Softmax function applied row-wise to the matrix Z
Matrix softmax(const Matrix& Z) {
    Matrix Y = Z;
    
    for (size_t i = 0; i < Z.size(); ++i) { // For each sample (row)
        const Vector& z_row = Z[i];
        Vector& y_row = Y[i];

        // 1. Find max value for numerical stability
        double max_val = z_row[0];
        for (double val : z_row) {
            if (val > max_val) max_val = val;
        }

        // 2. Calculate e^(z_i - max_val) and the sum
        double sum_exp = 0.0;
        for (size_t j = 0; j < z_row.size(); ++j) {
            double exp_val = std::exp(z_row[j] - max_val);
            y_row[j] = exp_val;
            sum_exp += exp_val;
        }

        // 3. Normalize to get probabilities
        for (size_t j = 0; j < y_row.size(); ++j) {
            y_row[j] /= sum_exp;
        }
    }
    return Y;
}

// @hsongaraVTW25: Manual Matrix Multiplication and Bias Addition

// Calculates Z = A * W_T + B (batched matrix multiplication)
// A: (k x H), W: (C x H), B: (C x 1). Result Z: (k x C)
Matrix mat_mul_add_batched(const Matrix& A, const Matrix& W, const Vector& b) {
    size_t k = A.size();         // Batch size (rows in A)
    size_t H = W[0].size();      // Inner dimension (cols in A, cols in W_T)
    size_t C = W.size();         // Output dimension (rows in W)

    if (A[0].size() != H || b.size() != C) {
        cerr << "Error: Dimension mismatch in mat_mul_add_batched." << endl;
        return {};
    }

    Matrix Z(k, Vector(C, 0.0)); // Initialize result matrix (k x C)

    // Standard Matrix Multiplication: Z[i][j] = sum(A[i][l] * W_T[l][j])
    // Since W is (C x H), W_T is (H x C). So W_T[l][j] is W[j][l].
    for (size_t i = 0; i < k; ++i) {    // For each sample in the batch (row i)
        for (size_t j = 0; j < C; ++j) { // For each output neuron (column j)
            double sum = 0.0;
            for (size_t l = 0; l < H; ++l) { // Over the inner dimension H
                sum += A[i][l] * W[j][l];
            }
            Z[i][j] = sum + b[j]; // Add the bias component
        }
    }
    
    return Z;
}

// @hsongaraVTW25: Neural Network Class (Standard C++)

class NN {
private:
    Matrix W1; // Hidden Weights (num_hidden x num_features)
    Vector b1; // Hidden Biases (num_hidden x 1)
    Matrix W2; // Output Weights (num_classes x num_hidden)
    Vector b2; // Output Biases (num_classes x 1)
    
public:
    // Constructor
    NN() {
        // W1 initialization (num_hidden x num_features)
        W1 = {
            {0.1, -0.2, 0.3, 0.05, 0.1, -0.1},
            {0.2, 0.1, -0.1, 0.25, 0.2, 0.0},
            {-0.3, 0.4, 0.1, -0.15, -0.1, 0.3},
            {0.15, -0.05, 0.2, 0.1, 0.0, 0.1},
            {-0.25, 0.1, 0.0, 0.3, 0.2, -0.1}
        };

        // b1 initialization (num_hidden x 1)
        b1 = {0.1, 0.2, -0.1, 0.05, 0.15};

        // W2 initialization (num_classes x num_hidden)
        W2 = {
            {-0.1, 0.2, 0.1, -0.3, 0.05},
            {0.05, -0.1, 0.2, 0.15, -0.2},
            {0.1, 0.0, -0.15, 0.25, 0.1},
            {-0.2, 0.1, 0.3, 0.0, -0.1}
        };

        // b2 initialization (num_classes x 1)
        b2 = {0.1, -0.1, 0.05, 0.2};
    }

    /**
     * Performs the forward pass for a batch of inputs X.
     * X is expected to be (k x num_features)
     * @return A vector containing one index (0 to k-1) of the input sample with highest confidence.
     */
    int predict(const Matrix& X) {
        // We transpose W1 and W2 in the call to mat_mul_add_batched to follow the
        // X * W^T + b convention, where X is (k x F) and W is (H x F).

        // --- 1. Hidden Layer ---
        // A1: (k x num_hidden) = X * W1_T + b1
        Matrix A1 = mat_mul_add_batched(X, W1, b1);
        A1 = relu(A1);

        // --- 2. Output Layer ---
        // Y: (k x num_classes) = A1 * W2_T + b2
        Matrix Y = mat_mul_add_batched(A1, W2, b2);
        Y = softmax(Y);

        // --- 3. Final Output: Find the sample index (0 to k-1) with highest confidence ---
        int best_sample_index = 0;
        double highest_confidence = 0.0;

        // Find the sample with the highest maximum probability
        for (size_t sample_idx = 0; sample_idx < Y.size(); ++sample_idx) {
            const auto& row = Y[sample_idx];
            
            // Find the max probability in this sample
            double max_prob_in_sample = row[0];
            for (size_t i = 1; i < row.size(); ++i) {
                if (row[i] > max_prob_in_sample) {
                    max_prob_in_sample = row[i];
                }
            }
            
            // Check if this sample has the highest confidence so far
            if (max_prob_in_sample > highest_confidence) {
                highest_confidence = max_prob_in_sample;
                best_sample_index = sample_idx;
            }
        }

        return best_sample_index;
    }
};