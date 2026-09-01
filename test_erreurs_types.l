int main()
    # Erreur : Types mixtes au sein d'une même liste
    invalid_list = [1, 2.5, "texte"]

    # Erreur : Décomposition '::' mal formée
    head:: = invalid_list

    return 0
