% ----------- PARTE 1: Entradas del usuario -----------
% Definir matriz de adyacencia (no dirigida)
A = [
    0 1 1 0 0 0 0 0;  % 1 -> {2,3}
    1 0 1 0 0 1 0 0;  % 2 -> {1,3,6}
    1 1 0 1 0 0 1 0;  % 3 -> {1,2,4,7}
    0 0 1 0 1 0 0 0;  % 4 -> {3,5}
    0 0 0 1 0 0 0 1;  % 5 -> {4,8}
    0 1 0 0 0 0 1 0;  % 6 -> {2,7}
    0 0 1 0 0 1 0 1;  % 7 -> {3,6,8}
    0 0 0 0 1 0 1 0   % 8 -> {5,7}
];

% Nodos que quieres eliminar (usa índices de 1 a n)
nodos_eliminar = [3];  % 

% ----------- PARTE 2: Cálculos algebraicos -----------
syms lambda

% Reducir la matriz de adyacencia
A_red = A;
A_red(nodos_eliminar,:) = [];
A_red(:,nodos_eliminar) = [];

% Calcular la Laplaciana: L = D - A
D_red = diag(sum(A_red,2));
L_red = D_red - A_red;

% Matriz identidad simbólica
n = size(L_red, 1);
I = sym(eye(n));

% Matriz característica
M = L_red - lambda * I;

% Polinomio característico
char_poly = simplify(det(M));

% Mostrar resultados
disp('Matriz Laplaciana reducida L:')
disp(L_red)

disp('Polinomio característico:')
pretty(char_poly)

% Autovalores simbólicos
eigenvals = solve(char_poly == 0, lambda);
disp('Autovalores simbólicos:')
disp(double(eigenvals))
