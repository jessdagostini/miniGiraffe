options(crayon.enabled = FALSE)
suppressMessages(library(tidyverse))
suppressMessages(library(stringr))

read_traces <- function(file) {
    read_csv(file,
             skip = 1,
             progress=FALSE,
             col_names=FALSE,
             col_types=cols()) %>%
        rename(Query = X1, Runtime = X2, Thread = X3)
}

FOLDER="/home/cc/miniGiraffe/iiswc25/intelxeonplatinum8260cpu@240ghz/tuning"
df <- tibble(SOURCE = list.files(FOLDER,
                                 pattern="csv",
                                 recursive=TRUE,
                                 full.names=TRUE)
             )
df %>%
  mutate(DATA = map(SOURCE, read_traces)) %>%
  separate(SOURCE, c("XX1", "XX2", "XX3", "XX4", "XX5", "XX6", "XX7", "InputSet", "EXP"), sep="/") %>%
  separate(EXP, c("Batches", "Threads", "Scheduler", "Repetition", "Test", "CacheSize"), sep="_") %>%
  mutate(CacheSize = str_replace_all(CacheSize, "([.csv])", "")) %>%
  select(-contains("XX")) %>%
  unnest(DATA) %>%
  mutate(Batches = as.integer(Batches),
         Threads = as.integer(Threads),
         Repetition = as.integer(Repetition),
         CacheSize = as.integer(CacheSize),
         InputSet = case_when(InputSet == "1000GP" ~ "A-human",
                              InputSet == "yeast" ~ "B-yeast",
                              InputSet == "chm13" ~ "D-HPRC",
                              InputSet == "grch38" ~ "C-HPRC")) -> df.7.1
df.7.1 %>%
  group_by(InputSet, Batches, Threads, Scheduler, Repetition, CacheSize, Test) %>%
  summarize(Makespan = max(Runtime)) -> df.7.1.makespan

df.7.1.makespan %>%
  ungroup() %>%
  group_by(InputSet, Threads) %>%
  filter(Makespan == min(Makespan),
         Threads != 72) %>%
  mutate(Setting = "tuned") -> df.7.1.best

df.7.1.makespan %>%
  ungroup() %>%
  group_by(InputSet, Threads) %>%
  filter(Batches == 512 & Scheduler == "omp" & CacheSize == 256 & Test == "tuning") %>%
  mutate(Setting = "original") -> df.7.1.original

df.7.1.original %>%
  bind_rows(df.7.1.best) %>%
  print() -> df.7.1.compare

improvement.7.1 <- df.7.1.compare %>%
  
  # 1. Select only the columns needed for this operation.
  # This defines the "key" (InputSet, Threads) for pairing and the values to reshape.
  select(InputSet, Threads, Setting, Makespan) %>%
  
  # 2. Reshape the data from a "long" to a "wide" format.
  # This creates separate columns for 'original' and 'tuned' Makespan values.
  pivot_wider(
    names_from = Setting,
    values_from = Makespan
  ) %>%
  
  # 3. Remove rows where a pair could not be found 
  # (e.g., an 'original' without a 'tuned' counterpart).
  filter(!is.na(original) & !is.na(tuned)) %>%
  
  # 4. Calculate the percentage improvement.
  # Formula: ((Original - Tuned) / Original) * 100
  mutate(
    Percent = ((original - tuned) / original) * 100
  )

df.7.1.compare %>%
  filter(Threads != 72) %>%
  left_join(improvement.7.1, by=c("InputSet", "Threads")) %>%
  select(-original, -tuned) %>%
  write_csv("./iiswc25/best_results_tuning_intel.csv") %>%
  print() -> df.7.1.compare

dodge_width <- 0.9
p <- df.7.1.compare %>%
  filter(Threads != 72) %>%
  mutate(CacheSize = as.factor(CacheSize),
         Threads = as.factor(Threads)) %>%
  mutate(Machine = "chi-intel") %>%
  ggplot(aes(x=Machine, y=Makespan, fill=Setting)) +
  geom_bar(stat = "identity", width = 0.8, position = "dodge") +
  # Text annotation layer
  geom_text(
    # This is the key part:
    # We filter the data for this layer to only include 'original' executions
    # that have a percentage greater than 0.
    data = . %>% filter(Setting == 'original', !is.na(Percent)),
      # Define the aesthetics for the label.
    # We format the 'Percent' column to one decimal place and add a '%' sign.
    aes(label = paste0(round(Percent, 1), "%")),
    
    # Dodge the text to align with the corresponding bars.
    # It's crucial that this matches the bar's dodge width.
    position = position_dodge(width = 0.9),
    
    # Adjust the vertical position to be slightly above the bar.
    vjust = 1.5,
    
    # Set the font size for readability.
    size = 6
  ) +
  facet_wrap(~InputSet, scales="free") +
  theme_bw() +
  theme(legend.position = "top",
        text = element_text(size = 18),
        axis.text.y = element_text(angle = 45, hjust = 1, size=15),
        axis.text.x = element_text(angle = 15, hjust = 0.5, size=15))

ggsave("auto-tuning-results.png", plot = p)
print("SIMILAR TO TABLE 8 RESULTS")
df.7.1.compare %>%
  filter(Setting == "tuned") %>%
  select(InputSet, Batches, Scheduler, CacheSize)
